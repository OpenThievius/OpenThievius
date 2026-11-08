// SPDX-License-Identifier: AGPL-3.0-or-later
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs::{self, OpenOptions},
    io::{self, Read, Write},
    path::{Path, PathBuf},
    time::{SystemTime, UNIX_EPOCH},
};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(default)]
pub struct LauncherConfig {
    pub disc_folder: Option<PathBuf>,
    pub game_ports: BTreeMap<String, PathBuf>,
    pub playable: BTreeMap<String, bool>,
    pub display_mode: BTreeMap<String, DisplayMode>,
    pub installed: BTreeMap<String, InstalledVersion>,
}

impl LauncherConfig {
    pub fn is_playable(&self, game_id: &str) -> bool {
        self.playable.get(game_id).copied().unwrap_or(true)
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct InstalledVersion {
    pub image: PathBuf,
    pub executable: PathBuf,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum DisplayMode {
    Windowed,
    BorderlessFullscreen,
}

impl Default for LauncherConfig {
    fn default() -> Self {
        Self {
            disc_folder: None,
            game_ports: BTreeMap::new(),
            playable: BTreeMap::new(),
            display_mode: BTreeMap::new(),
            installed: BTreeMap::new(),
        }
    }
}

impl Default for DisplayMode {
    fn default() -> Self {
        Self::Windowed
    }
}

pub fn data_root_or_recover() -> io::Result<(PathBuf, Option<String>)> {
    let pointer_path = data_root_pointer();
    let (root, status) = match fs::read(&pointer_path) {
        Ok(bytes) => match serde_json::from_slice::<DataRootPointer>(&bytes) {
            Ok(stored) if stored.path.is_absolute() => (stored.path, None),
            Ok(_) => recover_data_root_pointer(&pointer_path, "stored path is not absolute")?,
            Err(error) => recover_data_root_pointer(&pointer_path, &error.to_string())?,
        },
        Err(error) if error.kind() == io::ErrorKind::NotFound => (default_data_root()?, None),
        Err(error) => return Err(error),
    };
    create_layout(&root).map_err(|error| {
        io::Error::new(
            error.kind(),
            format!("could not initialize {}: {error}", root.display()),
        )
    })?;
    Ok((root, status))
}

fn recover_data_root_pointer(
    pointer_path: &Path,
    reason: &str,
) -> io::Result<(PathBuf, Option<String>)> {
    recover_data_root_pointer_to(pointer_path, reason, default_data_root()?)
}

fn recover_data_root_pointer_to(
    pointer_path: &Path,
    reason: &str,
    fallback_root: PathBuf,
) -> io::Result<(PathBuf, Option<String>)> {
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    let backup = pointer_path.with_file_name(format!(
        "{}.invalid-{timestamp}",
        pointer_path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
    ));
    fs::rename(pointer_path, &backup)?;
    Ok((
        fallback_root,
        Some(format!(
            "The saved data-folder pointer could not be read ({reason}). Its original was preserved at {}; the standard data folder is active. Check the backup if you used a custom data folder.",
            backup.display()
        )),
    ))
}

pub fn suggested_data_root() -> io::Result<PathBuf> {
    let data_dir = dirs::data_local_dir().ok_or_else(|| {
        io::Error::new(
            io::ErrorKind::NotFound,
            "application data directory unavailable",
        )
    })?;
    Ok(data_dir.join("openthievius"))
}

fn default_data_root() -> io::Result<PathBuf> {
    let root = suggested_data_root()?;
    fs::create_dir_all(&root)?;
    root.canonicalize()
}

fn create_layout(root: &Path) -> io::Result<()> {
    fs::create_dir_all(root)?;
    for game in ["sly1", "sly2", "sly3"] {
        fs::create_dir_all(root.join(game).join("saves"))?;
    }
    Ok(())
}

pub fn load(root: &Path) -> io::Result<LauncherConfig> {
    let path = root.join("config.json");
    match fs::read(path) {
        Ok(bytes) => serde_json::from_slice(&bytes)
            .map_err(|error| io::Error::new(io::ErrorKind::InvalidData, error)),
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(LauncherConfig::default()),
        Err(error) => Err(error),
    }
}

pub fn load_or_recover(root: &Path) -> io::Result<(LauncherConfig, Option<String>)> {
    match load(root) {
        Ok(config) => Ok((config, None)),
        Err(error) if error.kind() == io::ErrorKind::InvalidData => {
            let config_path = root.join("config.json");
            let timestamp = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_nanos();
            let backup = root.join(format!("config.json.invalid-{timestamp}"));
            fs::rename(&config_path, &backup)?;
            Ok((
                LauncherConfig::default(),
                Some(format!(
                    "Launcher settings could not be read ({error}). The original was preserved at {}; defaults are active.",
                    backup.display()
                )),
            ))
        }
        Err(error) => Err(error),
    }
}

pub fn save(root: &Path, config: &LauncherConfig) -> io::Result<()> {
    let path = root.join("config.json");
    let bytes = serde_json::to_vec_pretty(config)
        .map_err(|error| io::Error::new(io::ErrorKind::InvalidData, error))?;
    atomic_write(&path, &bytes)
}

pub fn atomic_write(path: &Path, bytes: &[u8]) -> io::Result<()> {
    let name = path
        .file_name()
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "file path has no name"))?;
    let mut temporary_file = None;
    for attempt in 0..100 {
        let temporary = path.with_file_name(format!(
            "{}.tmp-{}-{attempt}",
            name.to_string_lossy(),
            std::process::id()
        ));
        match OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary)
        {
            Ok(file) => {
                temporary_file = Some((temporary, file));
                break;
            }
            Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(error),
        }
    }
    let (temporary, mut file) = temporary_file.ok_or_else(|| {
        io::Error::new(
            io::ErrorKind::AlreadyExists,
            "could not reserve a temporary file for atomic write",
        )
    })?;
    if let Err(error) = file.write_all(bytes).and_then(|_| file.sync_all()) {
        drop(file);
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }
    drop(file);
    if let Err(error) = replace_file(&temporary, path) {
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }
    Ok(())
}

#[derive(Serialize, Deserialize)]
struct DataRootPointer {
    path: PathBuf,
}

pub struct DataMoveResult {
    pub path: PathBuf,
    pub cleanup_warning: Option<String>,
}

fn data_root_pointer() -> PathBuf {
    #[cfg(test)]
    if let Some(path) = TEST_POINTER_PATH.with(|path| path.borrow().clone()) {
        return path;
    }
    dirs::config_dir()
        .unwrap_or_else(|| dirs::data_local_dir().unwrap_or_else(|| PathBuf::from(".")))
        .join("openthievius")
        .join("data-root.json")
}

#[cfg(test)]
thread_local! {
    static TEST_POINTER_PATH: std::cell::RefCell<Option<PathBuf>> = const { std::cell::RefCell::new(None) };
}

pub fn move_data_root(old: &Path, requested_new: &Path) -> io::Result<DataMoveResult> {
    let old = old.canonicalize()?;
    let new = absolute_target(requested_new)?;
    if new == old {
        return Ok(DataMoveResult {
            path: old,
            cleanup_warning: None,
        });
    }
    if new.starts_with(&old) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "new data folder must be outside the current data folder",
        ));
    }
    let empty_existing_target = if new.exists() {
        if !new.is_dir() || fs::read_dir(&new)?.next().is_some() {
            return Err(io::Error::new(
                io::ErrorKind::AlreadyExists,
                "new data folder must be empty",
            ));
        }
        true
    } else {
        false
    };
    let parent = new.parent().ok_or_else(|| {
        io::Error::new(io::ErrorKind::InvalidInput, "new data folder has no parent")
    })?;
    fs::create_dir_all(parent)?;
    let stage = parent.join(format!(".openthievius-data-move-{}", std::process::id()));
    if stage.exists() {
        return Err(io::Error::new(
            io::ErrorKind::AlreadyExists,
            "a previous data move staging folder exists",
        ));
    }
    let result = copy_and_verify(&old, &stage).and_then(|_| {
        if empty_existing_target {
            fs::remove_dir(&new)?;
        }
        fs::rename(&stage, &new)
    });
    if let Err(error) = result {
        let _ = fs::remove_dir_all(&stage);
        return Err(error);
    }
    if let Err(error) = write_data_root_pointer(&new) {
        return Err(io::Error::new(
            error.kind(),
            format!(
                "could not save the new data folder pointer ({error}); the original remains at {} and a verified copy remains at {}",
                old.display(),
                new.display()
            ),
        ));
    }
    let cleanup_warning = fs::remove_dir_all(&old).err().map(|error| {
        format!(
            "the new data folder is active at {}, but the old folder remains at {} because cleanup failed: {error}",
            new.display(),
            old.display()
        )
    });
    Ok(DataMoveResult {
        path: new,
        cleanup_warning,
    })
}

fn absolute_target(path: &Path) -> io::Result<PathBuf> {
    if path.exists() {
        return path.canonicalize();
    }
    let absolute = if path.is_absolute() {
        path.to_owned()
    } else {
        std::env::current_dir()?.join(path)
    };
    let parent = absolute.parent().ok_or_else(|| {
        io::Error::new(io::ErrorKind::InvalidInput, "new data folder has no parent")
    })?;
    fs::create_dir_all(parent)?;
    Ok(parent.canonicalize()?.join(
        absolute.file_name().ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidInput, "invalid new data folder")
        })?,
    ))
}

fn write_data_root_pointer(path: &Path) -> io::Result<()> {
    let pointer_path = data_root_pointer();
    let parent = pointer_path.parent().expect("pointer path has parent");
    fs::create_dir_all(parent)?;
    let bytes = serde_json::to_vec_pretty(&DataRootPointer {
        path: path.to_owned(),
    })
    .map_err(|error| io::Error::new(io::ErrorKind::InvalidData, error))?;
    atomic_write(&pointer_path, &bytes)
}

fn copy_and_verify(old: &Path, stage: &Path) -> io::Result<()> {
    fs::create_dir(stage)?;
    for entry in walkdir::WalkDir::new(old).follow_links(false).min_depth(1) {
        let entry = entry.map_err(|error| io::Error::other(error.to_string()))?;
        let relative = entry.path().strip_prefix(old).map_err(io::Error::other)?;
        let destination = stage.join(relative);
        if entry.file_type().is_dir() {
            fs::create_dir_all(destination)?;
        } else if entry.file_type().is_file() {
            if let Some(parent) = destination.parent() {
                fs::create_dir_all(parent)?;
            }
            copy_file(entry.path(), &destination)?;
        } else {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!(
                    "unsupported file type in data folder: {}",
                    entry.path().display()
                ),
            ));
        }
    }
    verify_tree(old, stage)
}

fn copy_file(source: &Path, destination: &Path) -> io::Result<()> {
    fs::copy(source, destination)?;
    let mut original = fs::File::open(source)?;
    let mut copied = fs::File::open(destination)?;
    if digest(&mut original)? != digest(&mut copied)? {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("copy verification failed: {}", source.display()),
        ));
    }
    Ok(())
}

fn verify_tree(source: &Path, copied: &Path) -> io::Result<()> {
    for entry in walkdir::WalkDir::new(source).follow_links(false) {
        let entry = entry.map_err(|error| io::Error::other(error.to_string()))?;
        let relative = entry
            .path()
            .strip_prefix(source)
            .map_err(io::Error::other)?;
        let destination = copied.join(relative);
        if entry.file_type().is_dir() {
            if !destination.is_dir() {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "data folder directory verification failed",
                ));
            }
        } else if entry.file_type().is_file() {
            let mut original = fs::File::open(entry.path())?;
            let mut candidate = fs::File::open(destination)?;
            if digest(&mut original)? != digest(&mut candidate)? {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!(
                        "data folder verification failed: {}",
                        entry.path().display()
                    ),
                ));
            }
        } else {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "unsupported file type in data folder",
            ));
        }
    }
    Ok(())
}

fn digest(file: &mut fs::File) -> io::Result<[u8; 32]> {
    let mut hash = Sha256::new();
    let mut buffer = [0u8; 64 * 1024];
    loop {
        let count = file.read(&mut buffer)?;
        if count == 0 {
            break;
        }
        hash.update(&buffer[..count]);
    }
    Ok(hash.finalize().into())
}

#[cfg(not(windows))]
fn replace_file(source: &Path, destination: &Path) -> io::Result<()> {
    fs::rename(source, destination)
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn missing_and_malformed_config_are_recovered_without_data_loss() {
        let directory = tempdir().unwrap();
        let (config, status) = load_or_recover(directory.path()).unwrap();
        assert!(config.disc_folder.is_none());
        assert!(status.is_none());

        let config_path = directory.path().join("config.json");
        fs::write(&config_path, b"{ invalid json").unwrap();
        let (config, status) = load_or_recover(directory.path()).unwrap();
        assert!(config.disc_folder.is_none());
        let status = status.unwrap();
        let backup_path = status
            .split("preserved at ")
            .nth(1)
            .and_then(|message| message.split(';').next())
            .unwrap();
        assert_eq!(fs::read(backup_path).unwrap(), b"{ invalid json");
        assert!(!config_path.exists());
    }

    #[test]
    fn missing_playability_defaults_to_true_and_false_is_preserved() {
        let config: LauncherConfig =
            serde_json::from_str(r#"{"playable":{"sly1":false}}"#).unwrap();
        assert!(!config.is_playable("sly1"));
        assert!(config.is_playable("sly2"));
        assert!(config.is_playable("sly3"));
    }

    #[test]
    fn moves_data_only_after_verifying_copy() {
        let directory = tempdir().unwrap();
        TEST_POINTER_PATH.with(|path| {
            *path.borrow_mut() = Some(directory.path().join("config/data-root.json"));
        });
        let old = directory.path().join("old");
        let new = directory.path().join("new");
        fs::create_dir_all(old.join("sly1/saves")).unwrap();
        fs::write(old.join("config.json"), b"config").unwrap();
        fs::write(old.join("sly1/saves/save.dat"), b"save bytes").unwrap();
        let moved = move_data_root(&old, &new).unwrap();
        assert_eq!(moved.path, new.canonicalize().unwrap());
        assert!(moved.cleanup_warning.is_none());
        assert!(!old.exists());
        assert_eq!(fs::read(new.join("config.json")).unwrap(), b"config");
        assert_eq!(
            fs::read(new.join("sly1/saves/save.dat")).unwrap(),
            b"save bytes"
        );
        let pointer = TEST_POINTER_PATH.with(|path| path.borrow().clone().unwrap());
        let pointer: DataRootPointer = serde_json::from_slice(&fs::read(pointer).unwrap()).unwrap();
        assert_eq!(pointer.path, new.canonicalize().unwrap());
        TEST_POINTER_PATH.with(|path| *path.borrow_mut() = None);
    }

    #[test]
    fn rejects_nonempty_destination_without_touching_source_or_destination() {
        let directory = tempdir().unwrap();
        let old = directory.path().join("old");
        let new = directory.path().join("new");
        fs::create_dir_all(&old).unwrap();
        fs::create_dir_all(&new).unwrap();
        fs::write(old.join("important"), b"keep").unwrap();
        fs::write(new.join("do-not-overwrite"), b"also keep").unwrap();
        assert!(move_data_root(&old, &new).is_err());
        assert_eq!(fs::read(old.join("important")).unwrap(), b"keep");
        assert_eq!(
            fs::read(new.join("do-not-overwrite")).unwrap(),
            b"also keep"
        );
    }

    #[test]
    fn malformed_data_root_pointer_is_preserved_and_recovered() {
        let directory = tempdir().unwrap();
        let pointer = directory.path().join("data-root.json");
        let fallback = directory.path().join("default");
        fs::write(&pointer, b"not json").unwrap();
        let (root, status) =
            recover_data_root_pointer_to(&pointer, "invalid JSON", fallback.clone()).unwrap();
        assert_eq!(root, fallback);
        let status = status.unwrap();
        let backup = status
            .split("preserved at ")
            .nth(1)
            .and_then(|rest| rest.split(';').next())
            .unwrap();
        assert_eq!(fs::read(backup).unwrap(), b"not json");
        assert!(!pointer.exists());
    }
}

#[cfg(windows)]
fn replace_file(source: &Path, destination: &Path) -> io::Result<()> {
    use std::{iter, os::windows::ffi::OsStrExt};

    const MOVEFILE_REPLACE_EXISTING: u32 = 0x1;
    const MOVEFILE_WRITE_THROUGH: u32 = 0x8;
    #[link(name = "Kernel32")]
    unsafe extern "system" {
        fn MoveFileExW(existing: *const u16, new: *const u16, flags: u32) -> i32;
    }

    let source: Vec<u16> = source
        .as_os_str()
        .encode_wide()
        .chain(iter::once(0))
        .collect();
    let destination: Vec<u16> = destination
        .as_os_str()
        .encode_wide()
        .chain(iter::once(0))
        .collect();
    let result = unsafe {
        MoveFileExW(
            source.as_ptr(),
            destination.as_ptr(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH,
        )
    };
    if result == 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(())
    }
}
