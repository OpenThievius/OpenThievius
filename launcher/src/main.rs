// SPDX-License-Identifier: AGPL-3.0-or-later
mod config;
mod disc;

use config::{DisplayMode, InstalledVersion, LauncherConfig};
use disc::{ScanEvent, VersionDefinition};
use serde::Serialize;
use std::{
    path::PathBuf,
    process::Command,
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, Ordering},
        mpsc,
    },
    thread,
};
use tauri::{AppHandle, Emitter, State};

struct Core {
    data_root: PathBuf,
    config: LauncherConfig,
    definitions: Vec<VersionDefinition>,
    startup_status: Option<String>,
}

#[derive(Clone)]
struct LauncherState {
    core: Arc<Mutex<Core>>,
    scan_running: Arc<AtomicBool>,
    setup_running: Arc<AtomicBool>,
}

#[derive(Serialize)]
struct AppSnapshot {
    data_root: PathBuf,
    config: LauncherConfig,
    versions: Vec<VersionDefinition>,
    installed_versions: Vec<String>,
    startup_status: Option<String>,
}

#[derive(Clone, Serialize)]
struct ScanFinished {
    inspected: usize,
    errors: Vec<String>,
    found_count: usize,
}

#[derive(Clone, Serialize)]
struct SetupProgress {
    game_id: String,
    version_id: String,
    message: String,
}

#[derive(Clone, Serialize)]
struct SetupFinished {
    game_id: String,
    version_id: String,
    success: bool,
    message: String,
    installed: Option<InstalledVersion>,
}

#[derive(Clone, Serialize)]
struct LaunchFinished {
    game_id: String,
    success: bool,
    message: String,
}

#[tauri::command]
fn get_state(state: State<'_, LauncherState>) -> Result<AppSnapshot, String> {
    let mut core = state.core.lock().map_err(lock_error)?;
    let mut changed = false;
    let definitions = core.definitions.clone();
    for definition in &definitions {
        let key = version_key(&definition.game_id, &definition.id);
        let current_is_valid = core.config.installed.get(&key).is_some_and(|installed| {
            disc::boot_executable_matches(&installed.executable, definition)
        });
        if !current_is_valid {
            let executable = core
                .data_root
                .join(&definition.game_id)
                .join(&definition.id)
                .join("boot.elf");
            if disc::boot_executable_matches(&executable, definition) {
                let image = core
                    .config
                    .installed
                    .get(&key)
                    .map(|installed| installed.image.clone())
                    .unwrap_or_default();
                core.config
                    .installed
                    .insert(key, InstalledVersion { image, executable });
                changed = true;
            }
        }
    }
    if changed {
        config::save(&core.data_root, &core.config).map_err(|error| error.to_string())?;
    }
    let installed_versions = core
        .config
        .installed
        .iter()
        .filter_map(|(key, installed)| {
            core.definitions
                .iter()
                .find(|version| version_key(&version.game_id, &version.id) == *key)
                .filter(|version| disc::boot_executable_matches(&installed.executable, version))
                .map(|_| key.clone())
        })
        .collect();
    Ok(AppSnapshot {
        data_root: core.data_root.clone(),
        config: core.config.clone(),
        versions: core.definitions.clone(),
        installed_versions,
        startup_status: core.startup_status.clone(),
    })
}

#[tauri::command]
fn save_disc_folder(path: String, state: State<'_, LauncherState>) -> Result<(), String> {
    let folder = PathBuf::from(path);
    if !folder.is_dir() {
        return Err(format!("{} is not a folder", folder.display()));
    }
    let mut core = state.core.lock().map_err(lock_error)?;
    core.config.disc_folder = Some(folder);
    config::save(&core.data_root, &core.config).map_err(|error| error.to_string())
}

#[tauri::command]
fn set_playable(
    game_id: String,
    playable: bool,
    state: State<'_, LauncherState>,
) -> Result<(), String> {
    validate_game_id(&game_id)?;
    let mut core = state.core.lock().map_err(lock_error)?;
    core.config.playable.insert(game_id, playable);
    config::save(&core.data_root, &core.config).map_err(|error| error.to_string())
}

#[tauri::command]
fn set_display_mode(
    game_id: String,
    mode: DisplayMode,
    state: State<'_, LauncherState>,
) -> Result<(), String> {
    validate_game_id(&game_id)?;
    let mut core = state.core.lock().map_err(lock_error)?;
    core.config.display_mode.insert(game_id, mode);
    config::save(&core.data_root, &core.config).map_err(|error| error.to_string())
}

#[tauri::command]
fn set_game_port(
    game_id: String,
    path: String,
    state: State<'_, LauncherState>,
) -> Result<(), String> {
    validate_game_id(&game_id)?;
    let path = PathBuf::from(path);
    if !path.is_file() {
        return Err(format!("{} is not a file", path.display()));
    }
    let mut core = state.core.lock().map_err(lock_error)?;
    core.config.game_ports.insert(game_id, path);
    config::save(&core.data_root, &core.config).map_err(|error| error.to_string())
}

#[tauri::command]
async fn move_data_directory(
    path: String,
    state: State<'_, LauncherState>,
) -> Result<String, String> {
    let requested = PathBuf::from(path);
    let core = state.core.clone();
    tauri::async_runtime::spawn_blocking(move || {
        let mut core = core.lock().map_err(lock_error)?;
        let moved = config::move_data_root(&core.data_root, &requested)
            .map_err(|error| format!("Could not move the data directory: {error}"))?;
        core.data_root = moved.path.clone();
        if let Some(warning) = moved.cleanup_warning {
            Ok(format!("Data moved and verified. {warning}"))
        } else {
            Ok(format!(
                "Data moved and verified to {}.",
                moved.path.display()
            ))
        }
    })
    .await
    .map_err(|error| format!("Data move worker failed: {error}"))?
}

#[tauri::command]
fn scan_discs(app: AppHandle, state: State<'_, LauncherState>) -> Result<(), String> {
    let core = state.core.lock().map_err(lock_error)?;
    let Some(folder) = core.config.disc_folder.clone() else {
        return Err("Choose a disc image folder in Settings first.".to_owned());
    };
    let definitions = core.definitions.clone();
    drop(core);

    if state
        .scan_running
        .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
        .is_err()
    {
        return Err("A disc scan is already running.".to_owned());
    }
    let scan_running = state.scan_running.clone();
    thread::spawn(move || {
        let _reset = AtomicReset(scan_running);
        let (sender, receiver) = mpsc::channel();
        thread::spawn(move || disc::scan_folder(&folder, &definitions, sender));
        let mut found_count = 0;
        for event in receiver {
            match event {
                ScanEvent::Found(found) => {
                    found_count += 1;
                    let _ = app.emit("scan-version-found", found);
                }
                ScanEvent::Finished { inspected, errors } => {
                    let _ = app.emit(
                        "scan-finished",
                        ScanFinished {
                            inspected,
                            errors,
                            found_count,
                        },
                    );
                }
            }
        }
    });
    Ok(())
}

#[tauri::command]
fn setup_version(
    app: AppHandle,
    game_id: String,
    version_id: String,
    image: String,
    state: State<'_, LauncherState>,
) -> Result<(), String> {
    validate_game_id(&game_id)?;
    if state
        .setup_running
        .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
        .is_err()
    {
        return Err("Another version is being set up.".to_owned());
    }

    let setup_running = state.setup_running.clone();
    let core = state.core.clone();
    thread::spawn(move || {
        let _reset = AtomicReset(setup_running);
        let image = PathBuf::from(image);
        let definition = match core.lock() {
            Ok(core) => core
                .definitions
                .iter()
                .find(|item| item.game_id == game_id && item.id == version_id)
                .cloned(),
            Err(_) => None,
        };
        let Some(definition) = definition else {
            emit_setup_finished(
                &app,
                game_id,
                version_id.clone(),
                version_id,
                Err("The selected supported version is unavailable.".to_owned()),
            );
            return;
        };
        let label = definition.label.clone();
        let data_root = match core.lock() {
            Ok(core) => core.data_root.clone(),
            Err(_) => {
                emit_setup_finished(
                    &app,
                    game_id,
                    version_id.clone(),
                    version_id,
                    Err("Launcher settings are unavailable.".to_owned()),
                );
                return;
            }
        };

        emit_setup_progress(
            &app,
            &game_id,
            &version_id,
            "Reading and verifying the disc image…",
        );
        let result = (|| {
            let executable =
                disc::read_supported_boot_executable(&image, &definition).map_err(|error| {
                    format!("Could not read the supported boot executable: {error}")
                })?;
            let version_dir = data_root.join(&game_id).join(&version_id);
            std::fs::create_dir_all(&version_dir)
                .map_err(|error| format!("Could not create the game data folder: {error}"))?;
            let executable_path = version_dir.join("boot.elf");
            emit_setup_progress(
                &app,
                &game_id,
                &version_id,
                "Writing the verified executable…",
            );
            config::atomic_write(&executable_path, &executable)
                .map_err(|error| format!("Could not write the boot executable: {error}"))?;
            let installed = InstalledVersion {
                image: image.clone(),
                executable: executable_path,
            };
            let mut core = core
                .lock()
                .map_err(|_| "Launcher settings are unavailable".to_owned())?;
            core.config
                .installed
                .insert(version_key(&game_id, &version_id), installed.clone());
            config::save(&core.data_root, &core.config).map_err(|error| {
                format!("Setup completed but settings could not be saved: {error}")
            })?;
            Ok(installed)
        })();
        emit_setup_finished(&app, game_id, version_id, label, result);
    });
    Ok(())
}

fn emit_setup_progress(app: &AppHandle, game_id: &str, version_id: &str, message: &str) {
    let _ = app.emit(
        "setup-progress",
        SetupProgress {
            game_id: game_id.to_owned(),
            version_id: version_id.to_owned(),
            message: message.to_owned(),
        },
    );
}

fn emit_setup_finished(
    app: &AppHandle,
    game_id: String,
    version_id: String,
    label: String,
    result: Result<InstalledVersion, String>,
) {
    let (success, message, installed) = match result {
        Ok(installed) => (true, format!("{} is set up.", label), Some(installed)),
        Err(error) => (false, format!("Setup failed: {error}"), None),
    };
    let _ = app.emit(
        "setup-finished",
        SetupFinished {
            game_id,
            version_id,
            success,
            message,
            installed,
        },
    );
}

#[tauri::command]
fn launch_game(
    app: AppHandle,
    game_id: String,
    version_id: String,
    image: String,
    state: State<'_, LauncherState>,
) -> Result<(), String> {
    validate_game_id(&game_id)?;
    let mut core = state.core.lock().map_err(lock_error)?;
    if !core.config.is_playable(&game_id) {
        return Err("Play is disabled for this game in launcher settings.".to_owned());
    }
    let key = version_key(&game_id, &version_id);
    let installed = core
        .config
        .installed
        .get(&key)
        .cloned()
        .ok_or_else(|| "Set up this version before playing.".to_owned())?;
    let port = core
        .config
        .game_ports
        .get(&game_id)
        .cloned()
        .ok_or_else(|| "Choose the native game port in Settings before playing.".to_owned())?;
    if !port.is_file() {
        return Err(format!(
            "The configured game port is missing: {}",
            port.display()
        ));
    }
    let image = PathBuf::from(image);
    if !image.is_file() {
        return Err("The selected disc image is unavailable. Scan the folder again.".to_owned());
    }
    let definition = core
        .definitions
        .iter()
        .find(|item| item.game_id == game_id && item.id == version_id)
        .cloned()
        .ok_or_else(|| "The selected supported version is unavailable.".to_owned())?;
    let data_root = core.data_root.clone();
    if !disc::boot_executable_matches(&installed.executable, &definition) {
        return Err(
            "The installed boot executable no longer matches. Set up this version again."
                .to_owned(),
        );
    }
    let _ = disc::read_supported_boot_executable(&image, &definition)
        .map_err(|error| format!("The selected disc could not be verified: {error}"))?;
    let mode = core
        .config
        .display_mode
        .get(&game_id)
        .copied()
        .unwrap_or_default();
    let display_mode = match mode {
        DisplayMode::Windowed => "windowed",
        DisplayMode::BorderlessFullscreen => "borderless",
    };
    let previous_image = core
        .config
        .installed
        .get(&key)
        .map(|item| item.image.clone());
    if previous_image.as_deref() != Some(image.as_path()) {
        if let Some(current) = core.config.installed.get_mut(&key) {
            current.image = image.clone();
        }
        if let Err(error) = config::save(&core.data_root, &core.config) {
            let _ = app.emit(
                "launcher-status",
                format!("The selected disc path could not be saved: {error}"),
            );
        }
    }
    let mut child = Command::new(&port)
        .arg(&installed.executable)
        .arg(&image)
        .env("OPENTHIEVIUS_DISPLAY_MODE", display_mode)
        .env("OPENTHIEVIUS_GAME_ID", &game_id)
        .env("OPENTHIEVIUS_GAME_DATA_DIR", data_root.join(&game_id))
        .env(
            "OPENTHIEVIUS_SAVE_DIR",
            data_root.join(&game_id).join("saves"),
        )
        .env("OPENTHIEVIUS_WINDOW_WIDTH", "1280")
        .env("OPENTHIEVIUS_WINDOW_HEIGHT", "720")
        .spawn()
        .map_err(|error| format!("Could not launch the native port: {error}"))?;
    let _ = app.emit("launcher-status", format!("{version_id} is running…"));
    drop(core);
    thread::spawn(move || {
        let result = child.wait();
        let (success, message) = match result {
            Ok(status) => (status.success(), format!("Game exited with {status}.")),
            Err(error) => (
                false,
                format!("Could not monitor the game process: {error}"),
            ),
        };
        let _ = app.emit(
            "launch-finished",
            LaunchFinished {
                game_id,
                success,
                message,
            },
        );
    });
    Ok(())
}

fn validate_game_id(game_id: &str) -> Result<(), String> {
    if matches!(game_id, "sly1" | "sly2" | "sly3") {
        Ok(())
    } else {
        Err(format!("Unknown game: {game_id}"))
    }
}

fn version_key(game_id: &str, version_id: &str) -> String {
    format!("{game_id}:{version_id}")
}

fn lock_error<T>(_: std::sync::PoisonError<T>) -> String {
    "Launcher settings are unavailable.".to_owned()
}

struct AtomicReset(Arc<AtomicBool>);

impl Drop for AtomicReset {
    fn drop(&mut self) {
        self.0.store(false, Ordering::Release);
    }
}

fn main() {
    let (data_root, root_status) = config::data_root_or_recover()
        .unwrap_or_else(|error| panic!("could not initialize launcher data: {error}"));
    let (launcher_config, config_status) = config::load_or_recover(&data_root)
        .unwrap_or_else(|error| panic!("could not load launcher settings: {error}"));
    let startup_messages = root_status
        .into_iter()
        .chain(config_status)
        .collect::<Vec<_>>();
    let startup_status = (!startup_messages.is_empty()).then(|| startup_messages.join("\n"));
    let definitions: Vec<VersionDefinition> = serde_json::from_str(env!("SUPPORTED_VERSIONS"))
        .expect("embedded game version definitions");
    let state = LauncherState {
        core: Arc::new(Mutex::new(Core {
            data_root,
            config: launcher_config,
            definitions,
            startup_status,
        })),
        scan_running: Arc::new(AtomicBool::new(false)),
        setup_running: Arc::new(AtomicBool::new(false)),
    };

    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(state)
        .setup(|_app| Ok(()))
        .invoke_handler(tauri::generate_handler![
            get_state,
            save_disc_folder,
            set_playable,
            set_display_mode,
            set_game_port,
            move_data_directory,
            scan_discs,
            setup_version,
            launch_game,
        ])
        .run(tauri::generate_context!())
        .expect("error while running OpenThievius launcher");
}
