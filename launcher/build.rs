// SPDX-License-Identifier: AGPL-3.0-or-later
use serde_json::{Value, json};
use std::{
    collections::BTreeSet,
    env, fs,
    path::{Path, PathBuf},
};

fn main() {
    tauri_build::build();

    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("manifest dir"));
    let repo = manifest.parent().expect("launcher lives in repo root");
    let games_dir = repo.join("games");
    println!("cargo:rerun-if-changed={}", games_dir.display());

    let mut game_files = fs::read_dir(&games_dir)
        .expect("read games directory")
        .map(|entry| {
            entry
                .expect("read game directory entry")
                .path()
                .join("game.json")
        })
        .filter(|path| path.is_file())
        .collect::<Vec<_>>();
    game_files.sort();

    let mut definitions = Vec::new();
    for game_file in game_files {
        println!("cargo:rerun-if-changed={}", game_file.display());
        let game: Value = read_json(&game_file);
        let game_id = string(&game, &["id"]);
        verify_project_definitions(repo, &game);

        let versions = game["versions"]
            .as_object()
            .unwrap_or_else(|| panic!("{} has no versions object", game_file.display()));
        for (id, version) in versions {
            if version["role"].as_str() == Some("reference") {
                continue;
            }
            let boot = &version["boot"];
            let boot_name = boot["name"]
                .as_str()
                .unwrap_or_else(|| panic!("{game_id}/{id} has no boot name"));
            let boot_sha1 = boot["sha1"]
                .as_str()
                .unwrap_or_else(|| panic!("{game_id}/{id} has no boot SHA-1"));
            let label = version["label"]
                .as_str()
                .unwrap_or_else(|| panic!("{game_id}/{id} has no label"));
            definitions.push(json!({
                "game_id": game_id,
                "id": id,
                "label": label,
                "boot_name": boot_name,
                "boot_sha1": boot_sha1,
            }));
        }
    }

    let serialized = serde_json::to_string(&definitions).expect("serialize definitions");
    println!("cargo:rustc-env=SUPPORTED_VERSIONS={serialized}");
}

fn verify_project_definitions(repo: &Path, game: &Value) {
    let project = &game["project"];
    let game_id = string(game, &["id"]);
    let env_name = project["env"]
        .as_str()
        .unwrap_or_else(|| panic!("{game_id} has no project environment key"));
    let project_root = env::var_os(env_name).map(PathBuf::from).unwrap_or_else(|| {
        let path = Path::new(
            project["dir"]
                .as_str()
                .unwrap_or_else(|| panic!("{game_id} has no project directory")),
        );
        if path.is_absolute() {
            path.to_owned()
        } else {
            repo.join(path)
        }
    });
    let versions_path = project_root.join(
        project["versions_file"]
            .as_str()
            .unwrap_or_else(|| panic!("{game_id} has no project versions file")),
    );
    println!("cargo:rerun-if-env-changed={env_name}");
    println!("cargo:rerun-if-changed={}", versions_path.display());

    if !versions_path.is_file() {
        println!(
            "cargo:warning={game_id} project definitions are unavailable at {}; using games/{game_id}/game.json",
            versions_path.display()
        );
        return;
    }
    let project_versions: Value = read_json(&versions_path);
    assert_eq!(
        game["primary"], project_versions["primary"],
        "{game_id} primary version differs between the registry and project"
    );

    let registry = game["versions"].as_object().expect("registry versions");
    let project = project_versions["versions"]
        .as_object()
        .unwrap_or_else(|| panic!("{game_id} project has no versions object"));
    let registry_ids = registry.keys().collect::<BTreeSet<_>>();
    let project_ids = project.keys().collect::<BTreeSet<_>>();
    assert_eq!(
        registry_ids, project_ids,
        "{game_id} version IDs differ between the registry and project"
    );

    for (id, source) in project {
        let target = &registry[id];
        assert_eq!(
            source["role"], target["role"],
            "{game_id}/{id} role differs"
        );
        if source["role"].as_str() == Some("reference") {
            continue;
        }
        assert_eq!(
            source["boot"]["name"], target["boot"]["name"],
            "{game_id}/{id} boot name differs"
        );
        assert_eq!(
            source["boot"]["sha1"], target["boot"]["sha1"],
            "{game_id}/{id} boot SHA-1 differs"
        );
    }
}

fn read_json(path: &Path) -> Value {
    serde_json::from_slice(
        &fs::read(path).unwrap_or_else(|error| panic!("read {}: {error}", path.display())),
    )
    .unwrap_or_else(|error| panic!("parse {}: {error}", path.display()))
}

fn string<'a>(value: &'a Value, path: &[&str]) -> &'a str {
    path.iter()
        .fold(value, |current, key| &current[*key])
        .as_str()
        .unwrap_or_else(|| panic!("missing string field {}", path.join(".")))
}
