// SPDX-License-Identifier: AGPL-3.0-or-later
use serde::Deserialize;
use serde_json::value::RawValue;
use std::{
    env, fs,
    path::{Path, PathBuf},
};

#[derive(Deserialize)]
struct Game {
    id: String,
    primary: String,
    project: Project,
    versions: std::collections::BTreeMap<String, Version>,
}

#[derive(Deserialize)]
struct Project {
    env: String,
    dir: String,
    versions_file: String,
}

#[derive(Deserialize)]
struct Version {
    role: String,
    label: Box<RawValue>,
    boot: Box<RawValue>,
}

#[derive(Deserialize)]
struct ProjectVersions {
    primary: String,
    versions: std::collections::BTreeMap<String, ProjectVersion>,
}

#[derive(Deserialize)]
struct ProjectVersion {
    role: String,
    boot: Box<RawValue>,
}

#[derive(Deserialize, serde::Serialize)]
struct Boot {
    name: String,
    sha1: String,
}

#[derive(serde::Serialize)]
struct LauncherVersion {
    game_id: String,
    id: String,
    label: String,
    boot_name: String,
    boot_sha1: String,
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("manifest dir"));
    let repo = manifest.parent().expect("launcher lives in repo root");
    let games_dir = repo.join("games");
    println!("cargo:rerun-if-changed={}", games_dir.display());
    let mut game_jsons = fs::read_dir(&games_dir)
        .expect("read games directory")
        .map(|entry| {
            entry
                .expect("read game directory entry")
                .path()
                .join("game.json")
        })
        .filter(|path| path.is_file())
        .collect::<Vec<_>>();
    game_jsons.sort();

    let mut versions = Vec::new();
    for game_json in game_jsons {
        println!("cargo:rerun-if-changed={}", game_json.display());
        let game: Game = serde_json::from_slice(&fs::read(&game_json).expect("read game.json"))
            .expect("parse game.json");
        let project_versions = project_versions(repo, &game.id, &game).unwrap_or_else(|| {
            panic!(
                "{} project definitions are required to build launcher disc recognition",
                game.id
            )
        });
        for (id, version) in game.versions {
            if version.role == "reference" {
                continue;
            }
            let label: String =
                serde_json::from_str(version.label.get()).expect("parse retail version label");
            let boot: Boot =
                serde_json::from_str(version.boot.get()).expect("parse retail boot definition");
            let project_version = project_versions.versions.get(&id).unwrap_or_else(|| {
                panic!(
                    "{} version {id} is missing from its project's config/versions.json",
                    game.id
                )
            });
            assert_eq!(
                project_version.role, version.role,
                "{} version {id} role differs from the project definition",
                game.id
            );
            let project_boot: Boot = serde_json::from_str(project_version.boot.get())
                .expect("parse project retail boot definition");
            assert_eq!(
                project_boot.name, boot.name,
                "{} version {id} boot name differs from the project definition",
                game.id
            );
            assert_eq!(
                project_boot.sha1, boot.sha1,
                "{} version {id} boot SHA-1 differs from the project definition",
                game.id
            );
            versions.push(LauncherVersion {
                game_id: game.id.clone(),
                id,
                label,
                boot_name: boot.name,
                boot_sha1: boot.sha1,
            });
        }
    }
    let versions = serde_json::to_string(&versions).expect("serialize game version definitions");
    println!("cargo:rustc-env=SUPPORTED_VERSIONS={versions}");
}

fn project_versions(repo: &Path, game_id: &str, game: &Game) -> Option<ProjectVersions> {
    let env_name = &game.project.env;
    let configured_root = env::var_os(env_name).map(PathBuf::from);
    let project_root = configured_root.unwrap_or_else(|| {
        let relative = Path::new(&game.project.dir);
        if relative.is_absolute() {
            relative.to_owned()
        } else {
            repo.join(relative)
        }
    });
    let versions_path = project_root.join(&game.project.versions_file);
    println!("cargo:rerun-if-env-changed={env_name}");
    println!("cargo:rerun-if-changed={}", versions_path.display());
    if !versions_path.is_file() {
        println!(
            "cargo:warning={} project definitions unavailable at {}",
            game_id,
            versions_path.display()
        );
        return None;
    }
    let versions: ProjectVersions = serde_json::from_slice(
        &fs::read(&versions_path).expect("read project version definitions"),
    )
    .expect("parse project version definitions");
    assert_eq!(
        versions.primary, game.primary,
        "{} primary version differs from its project definition",
        game_id
    );
    let registry_ids = game
        .versions
        .keys()
        .collect::<std::collections::BTreeSet<_>>();
    let project_ids = versions
        .versions
        .keys()
        .collect::<std::collections::BTreeSet<_>>();
    assert_eq!(
        registry_ids, project_ids,
        "{} version IDs differ between project definition and game.json",
        game_id
    );
    for (id, project_version) in &versions.versions {
        let Some(registry_version) = game.versions.get(id) else {
            panic!(
                "{} project version {id} is missing from games/{}/game.json",
                game_id, game_id
            );
        };
        assert_eq!(
            project_version.role, registry_version.role,
            "{} version {id} role differs between project definition and game.json",
            game_id
        );
        if project_version.role == "reference" {
            continue;
        }
        let project_boot: Boot = serde_json::from_str(project_version.boot.get())
            .expect("parse project version boot definition");
        let registry_boot: Boot = serde_json::from_str(registry_version.boot.get())
            .expect("parse registry version boot definition");
        assert_eq!(
            project_boot.name, registry_boot.name,
            "{} version {id} boot name differs between project definition and game.json",
            game_id
        );
        assert_eq!(
            project_boot.sha1, registry_boot.sha1,
            "{} version {id} boot SHA-1 differs between project definition and game.json",
            game_id
        );
    }
    Some(versions)
}
