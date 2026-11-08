// SPDX-License-Identifier: AGPL-3.0-or-later
use serde::Deserialize;
use sha1::{Digest, Sha1};
use std::{
    fs::File,
    io::{self, Read, Seek, SeekFrom},
    path::{Path, PathBuf},
    sync::mpsc::Sender,
};
use walkdir::WalkDir;

const ISO_SECTOR: u64 = 2048;
const RAW_SECTOR: u64 = 2352;
const MAX_READ_SIZE: u32 = 128 * 1024 * 1024;
const MAX_SYSTEM_CNF_SIZE: u32 = 64 * 1024;
const MAX_BOOT_PATH_SIZE: usize = 1024;
const MAX_BOOT_PATH_COMPONENTS: usize = 8;
const RAW_SYNC: [u8; 12] = [0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0];

#[derive(Clone, Debug, Deserialize)]
pub struct VersionDefinition {
    pub game_id: String,
    pub id: String,
    pub label: String,
    pub boot_name: String,
    pub boot_sha1: String,
}

#[derive(Clone, Debug)]
pub struct FoundVersion {
    pub game_id: String,
    pub version_id: String,
    pub label: String,
    pub image: PathBuf,
}

#[derive(Clone, Debug)]
pub enum ScanEvent {
    Found(FoundVersion),
    Finished {
        inspected: usize,
        errors: Vec<String>,
    },
}

pub fn scan_folder(root: &Path, definitions: &[VersionDefinition], sender: Sender<ScanEvent>) {
    let mut inspected = 0;
    let mut errors = Vec::new();

    if !root.is_dir() {
        let detail = match std::fs::metadata(root) {
            Ok(_) => format!("{} is not a directory", root.display()),
            Err(error) => format!("{}: {error}", root.display()),
        };
        let _ = sender.send(ScanEvent::Finished {
            inspected,
            errors: vec![detail],
        });
        return;
    }

    for entry in WalkDir::new(root)
        .follow_links(false)
        .into_iter()
        .filter_entry(|entry| entry.file_name() != ".git")
    {
        let entry = match entry {
            Ok(entry) => entry,
            Err(error) => {
                if errors.len() < 25 {
                    errors.push(error.to_string());
                }
                continue;
            }
        };
        if !entry.file_type().is_file() || !is_disc_extension(entry.path()) {
            continue;
        }
        inspected += 1;
        match identify(entry.path(), definitions) {
            Ok(Some(version)) => {
                let _ = sender.send(ScanEvent::Found(FoundVersion {
                    game_id: version.game_id.clone(),
                    version_id: version.id.clone(),
                    label: version.label.clone(),
                    image: entry.path().to_owned(),
                }));
            }
            Ok(None) => {}
            Err(error) if error.kind() != io::ErrorKind::InvalidData && errors.len() < 25 => {
                errors.push(format!("{}: {error}", entry.path().display()))
            }
            Err(_) => {}
        }
    }
    let _ = sender.send(ScanEvent::Finished { inspected, errors });
}

fn is_disc_extension(path: &Path) -> bool {
    matches!(
        path.extension()
            .and_then(|extension| extension.to_str())
            .map(str::to_ascii_lowercase)
            .as_deref(),
        Some("iso" | "img" | "bin")
    )
}

fn identify<'a>(
    path: &Path,
    definitions: &'a [VersionDefinition],
) -> io::Result<Option<&'a VersionDefinition>> {
    let (boot_name, data) = read_boot_executable(path)?;
    let digest = format!("{:x}", Sha1::digest(&data));
    Ok(definitions
        .iter()
        .find(|version| version.boot_name == boot_name && version.boot_sha1 == digest))
}

pub fn read_boot_executable(path: &Path) -> io::Result<(String, Vec<u8>)> {
    let mut disc = DiscImage::open(path)?;
    let boot_name = disc.boot_name()?;
    let data = disc.file(&boot_name)?;
    Ok((boot_name, data))
}

pub fn read_supported_boot_executable(
    path: &Path,
    version: &VersionDefinition,
) -> io::Result<Vec<u8>> {
    let (boot_name, data) = read_boot_executable(path)?;
    let digest = format!("{:x}", Sha1::digest(&data));
    if version.boot_name != boot_name || version.boot_sha1 != digest {
        return Err(invalid_image(format!(
            "boot executable no longer matches supported version {}",
            version.label
        )));
    }
    Ok(data)
}

pub fn boot_executable_matches(path: &Path, version: &VersionDefinition) -> bool {
    let Ok(mut file) = File::open(path) else {
        return false;
    };
    if file
        .metadata()
        .map(|metadata| metadata.len() > MAX_READ_SIZE as u64)
        .unwrap_or(true)
    {
        return false;
    }
    let mut digest = Sha1::new();
    let mut buffer = [0; 64 * 1024];
    loop {
        let Ok(count) = file.read(&mut buffer) else {
            return false;
        };
        if count == 0 {
            break;
        }
        digest.update(&buffer[..count]);
    }
    format!("{:x}", digest.finalize()) == version.boot_sha1
}

struct DiscImage {
    file: File,
    raw: bool,
    root: Record,
}

#[derive(Clone)]
struct Extent {
    lba: u32,
    size: u32,
    extended_attribute_blocks: u8,
    unit_size: u8,
    gap_size: u8,
}

#[derive(Clone)]
struct Record {
    name: String,
    extents: Vec<Extent>,
    directory: bool,
    more_extents: bool,
}

impl DiscImage {
    fn open(path: &Path) -> io::Result<Self> {
        let mut file = File::open(path)?;
        let size = file.metadata()?.len();
        let mut sync = [0; 12];
        file.seek(SeekFrom::Start(16 * RAW_SECTOR))?;
        file.read_exact(&mut sync).map_err(invalid_image)?;
        let raw = size % RAW_SECTOR == 0 && sync == RAW_SYNC;
        let pvd = read_sector(&mut file, raw, 16)?;
        if pvd.get(0) != Some(&1) || pvd.get(1..6) != Some(b"CD001") {
            return Err(invalid_image(
                "no ISO 9660 primary volume descriptor at sector 16",
            ));
        }
        let block_size = read_both_u16(
            pvd.get(128..132)
                .ok_or_else(|| invalid_image("truncated volume descriptor"))?,
        )?;
        if block_size as u64 != ISO_SECTOR {
            return Err(invalid_image("unsupported ISO logical block size"));
        }
        let root = parse_record(
            pvd.get(156..190)
                .ok_or_else(|| invalid_image("truncated volume descriptor"))?,
        )?;
        if !root.directory {
            return Err(invalid_image("ISO root record is not a directory"));
        }
        Ok(Self { file, raw, root })
    }

    fn boot_name(&mut self) -> io::Result<String> {
        let root = self.root.clone();
        let system_cnf = self.find(&root, "SYSTEM.CNF")?;
        let system_cnf_size = system_cnf
            .extents
            .iter()
            .try_fold(0u64, |total, extent| total.checked_add(extent.size as u64))
            .ok_or_else(|| invalid_image("SYSTEM.CNF size overflow"))?;
        if system_cnf_size > MAX_SYSTEM_CNF_SIZE as u64 {
            return Err(invalid_image("SYSTEM.CNF is larger than expected"));
        }
        let bytes = self.read_record(&system_cnf)?;
        let text = String::from_utf8_lossy(&bytes);
        let boot = text
            .lines()
            .find_map(|line| {
                let (key, value) = line.split_once('=')?;
                (key.trim() == "BOOT2").then(|| value.trim())
            })
            .ok_or_else(|| invalid_image("SYSTEM.CNF has no BOOT2 entry"))?;
        // Match tools/disc.py: retain the complete ISO path after the device
        // prefix so a nested path cannot be mistaken for a root boot file.
        let name = boot
            .split_once(':')
            .map(|(_, path)| path)
            .unwrap_or(boot)
            .trim_start_matches(['\\', '/']);
        let name = name.split(';').next().unwrap_or(name);
        if name.is_empty() {
            return Err(invalid_image("SYSTEM.CNF names no boot executable"));
        }
        if name.len() > MAX_BOOT_PATH_SIZE {
            return Err(invalid_image("SYSTEM.CNF boot path is too long"));
        }
        Ok(name.to_owned())
    }

    fn file(&mut self, name: &str) -> io::Result<Vec<u8>> {
        let root = self.root.clone();
        let record = self.find(&root, name)?;
        if record.directory {
            return Err(invalid_image("BOOT2 path points to a directory"));
        }
        self.read_record(&record)
    }

    fn find(&mut self, directory: &Record, path: &str) -> io::Result<Record> {
        if path.len() > MAX_BOOT_PATH_SIZE {
            return Err(invalid_image("ISO file path is too long"));
        }
        let mut current = directory.clone();
        let normalized = path.replace('\\', "/");
        let components = normalized.trim_matches('/').split('/').collect::<Vec<_>>();
        if components.len() > MAX_BOOT_PATH_COMPONENTS {
            return Err(invalid_image("ISO file path is nested too deeply"));
        }
        for (component_index, component) in components.iter().enumerate() {
            if component.len() > 255 {
                return Err(invalid_image("ISO file path component is too long"));
            }
            let mut entries = self.directory(&current)?;
            let index = entries
                .iter()
                .position(|entry| entry.name.eq_ignore_ascii_case(component))
                .ok_or_else(|| invalid_image(format!("{path} not found on disc")))?;
            current = entries.remove(index);
            while current.more_extents {
                if index >= entries.len() {
                    return Err(invalid_image(format!(
                        "{path} has an incomplete multi-extent file"
                    )));
                }
                let next = entries.remove(index);
                if !next.name.eq_ignore_ascii_case(component)
                    || next.directory != current.directory
                    || next.extents.len() != 1
                {
                    return Err(invalid_image(format!(
                        "{path} has inconsistent multi-extent records"
                    )));
                }
                current.more_extents = next.more_extents;
                current.extents.extend(next.extents);
            }
            if component_index + 1 < components.len() && !current.directory {
                return Err(invalid_image(format!(
                    "{component} is not a directory in {path}"
                )));
            }
        }
        Ok(current)
    }

    fn directory(&mut self, directory: &Record) -> io::Result<Vec<Record>> {
        if !directory.directory {
            return Err(invalid_image("expected directory in ISO path"));
        }
        let bytes = self.read_record(directory)?;
        let mut result = Vec::new();
        let mut offset = 0;
        while offset < bytes.len() {
            let length = bytes[offset] as usize;
            if length == 0 {
                offset = (offset / ISO_SECTOR as usize + 1) * ISO_SECTOR as usize;
                continue;
            }
            let end = offset
                .checked_add(length)
                .filter(|end| *end <= bytes.len())
                .ok_or_else(|| invalid_image("truncated ISO directory record"))?;
            let record = parse_record(&bytes[offset..end])?;
            if record.name != "\0" && record.name != "\x01" {
                result.push(record);
            }
            offset = end;
        }
        Ok(result)
    }

    fn read_record(&mut self, record: &Record) -> io::Result<Vec<u8>> {
        let total = record
            .extents
            .iter()
            .try_fold(0u64, |total, extent| total.checked_add(extent.size as u64));
        let total = total.ok_or_else(|| invalid_image("ISO file size overflow"))?;
        if total > MAX_READ_SIZE as u64 {
            return Err(invalid_image(
                "ISO file is larger than the launcher can inspect",
            ));
        }
        let mut output = Vec::with_capacity(total as usize);
        for extent in &record.extents {
            if extent.unit_size == 0 && extent.gap_size == 0 {
                let data_lba = extent
                    .lba
                    .checked_add(extent.extended_attribute_blocks as u32)
                    .ok_or_else(|| invalid_image("ISO extent location overflow"))?;
                output.extend_from_slice(&self.read_extent(data_lba, extent.size)?);
            } else if extent.unit_size == 0 {
                return Err(invalid_image("invalid ISO file interleave fields"));
            } else {
                let sectors = (extent.size as u64).div_ceil(ISO_SECTOR);
                let mut extent_read = 0usize;
                if extent.extended_attribute_blocks > 0
                    && extent.extended_attribute_blocks != extent.unit_size
                {
                    return Err(invalid_image(
                        "interleaved extended attributes do not fill the first file unit",
                    ));
                }
                for index in 0..sectors {
                    let unit = extent.unit_size as u64;
                    let stride = unit + extent.gap_size as u64;
                    let mut lba = extent.lba as u64
                        + extent.extended_attribute_blocks as u64
                        + (index / unit) * stride
                        + (index % unit);
                    if extent.extended_attribute_blocks > 0 {
                        lba += extent.gap_size as u64;
                    }
                    let mut sector = read_sector(&mut self.file, self.raw, lba)?;
                    let remaining = extent.size as usize - extent_read;
                    sector.truncate(remaining.min(sector.len()));
                    extent_read += sector.len();
                    output.extend_from_slice(&sector);
                }
                if extent_read != extent.size as usize {
                    return Err(invalid_image(
                        "interleaved extent extends past end of image",
                    ));
                }
            }
        }
        if output.len() != total as usize {
            return Err(invalid_image("file extent extends past end of image"));
        }
        Ok(output)
    }

    fn read_extent(&mut self, lba: u32, size: u32) -> io::Result<Vec<u8>> {
        if size > MAX_READ_SIZE {
            return Err(invalid_image(
                "ISO file is larger than the launcher can inspect",
            ));
        }
        let mut output = Vec::with_capacity(size as usize);
        let sectors = (size as u64).div_ceil(ISO_SECTOR);
        for index in 0..sectors {
            let mut sector = read_sector(&mut self.file, self.raw, lba as u64 + index)?;
            let remaining = size as usize - output.len();
            sector.truncate(remaining.min(sector.len()));
            output.extend_from_slice(&sector);
        }
        if output.len() != size as usize {
            return Err(invalid_image("file extent extends past end of image"));
        }
        Ok(output)
    }
}

fn read_sector(file: &mut File, raw: bool, lba: u64) -> io::Result<Vec<u8>> {
    let stride = if raw { RAW_SECTOR } else { ISO_SECTOR };
    let base = lba
        .checked_mul(stride)
        .ok_or_else(|| invalid_image("sector offset overflow"))?;
    file.seek(SeekFrom::Start(base))?;
    let header_size = if raw {
        let mut header = [0; 24];
        file.read_exact(&mut header).map_err(invalid_image)?;
        if header[..12] != RAW_SYNC || !matches!(header[15], 1 | 2) {
            return Err(invalid_image("invalid raw CD sector header"));
        }
        if header[15] == 2 && header[16..20] != header[20..24] {
            return Err(invalid_image("inconsistent raw CD sector subheader"));
        }
        if header[15] == 2 { 24 } else { 16 }
    } else {
        0
    };
    let data_offset = base
        .checked_add(header_size)
        .ok_or_else(|| invalid_image("sector offset overflow"))?;
    file.seek(SeekFrom::Start(data_offset))?;
    let mut sector = vec![0; ISO_SECTOR as usize];
    file.read_exact(&mut sector).map_err(invalid_image)?;
    Ok(sector)
}

fn parse_record(bytes: &[u8]) -> io::Result<Record> {
    if bytes.len() < 34 || bytes[0] as usize != bytes.len() {
        return Err(invalid_image("invalid ISO directory record"));
    }
    let extent = read_both_u32(&bytes[2..10])?;
    let extended_attribute_blocks = bytes[1] as u32;
    let _data_lba = extent
        .checked_add(extended_attribute_blocks)
        .ok_or_else(|| invalid_image("ISO extent location overflow"))?;
    let size = read_both_u32(&bytes[10..18])?;
    let flags = bytes[25];
    let unit_size = bytes[26];
    let gap_size = bytes[27];
    let name_size = bytes[32] as usize;
    if 33 + name_size > bytes.len() {
        return Err(invalid_image("truncated ISO filename"));
    }
    let raw_name = &bytes[33..33 + name_size];
    let name = if raw_name == [0] {
        "\0".to_owned()
    } else if raw_name == [1] {
        "\x01".to_owned()
    } else {
        String::from_utf8_lossy(raw_name)
            .split(';')
            .next()
            .unwrap_or("")
            .to_owned()
    };
    Ok(Record {
        name,
        extents: vec![Extent {
            lba: extent,
            size,
            extended_attribute_blocks: extended_attribute_blocks as u8,
            unit_size,
            gap_size,
        }],
        directory: flags & 2 != 0,
        more_extents: flags & 0x80 != 0,
    })
}

fn read_both_u16(bytes: &[u8]) -> io::Result<u16> {
    if bytes.len() != 4 {
        return Err(invalid_image("invalid ISO both-endian 16-bit value"));
    }
    let little = u16::from_le_bytes(bytes[..2].try_into().unwrap());
    let big = u16::from_be_bytes(bytes[2..].try_into().unwrap());
    if little != big {
        return Err(invalid_image("inconsistent ISO both-endian 16-bit value"));
    }
    Ok(little)
}

fn read_both_u32(bytes: &[u8]) -> io::Result<u32> {
    if bytes.len() != 8 {
        return Err(invalid_image("invalid ISO both-endian 32-bit value"));
    }
    let little = u32::from_le_bytes(bytes[..4].try_into().unwrap());
    let big = u32::from_be_bytes(bytes[4..].try_into().unwrap());
    if little != big {
        return Err(invalid_image("inconsistent ISO both-endian 32-bit value"));
    }
    Ok(little)
}

fn invalid_image(error: impl std::fmt::Display) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, error.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{fs, sync::mpsc};
    use tempfile::tempdir;

    const TEST_BOOT: &[u8] = b"synthetic boot executable used only by launcher tests";

    fn le_be16(value: u16) -> [u8; 4] {
        [
            value as u8,
            (value >> 8) as u8,
            (value >> 8) as u8,
            value as u8,
        ]
    }

    fn le_be32(value: u32) -> [u8; 8] {
        [
            value as u8,
            (value >> 8) as u8,
            (value >> 16) as u8,
            (value >> 24) as u8,
            (value >> 24) as u8,
            (value >> 16) as u8,
            (value >> 8) as u8,
            value as u8,
        ]
    }

    fn record(name: &[u8], lba: u32, size: u32, directory: bool) -> Vec<u8> {
        let mut result = vec![0; 33 + name.len() + usize::from(name.len() % 2 == 0)];
        result[0] = result.len() as u8;
        result[2..10].copy_from_slice(&le_be32(lba));
        result[10..18].copy_from_slice(&le_be32(size));
        result[25] = if directory { 2 } else { 0 };
        result[28..32].copy_from_slice(&le_be16(1));
        result[32] = name.len() as u8;
        result[33..33 + name.len()].copy_from_slice(name);
        result
    }

    fn add_record(directory: &mut [u8], record: &[u8], offset: &mut usize) {
        if (*offset / 2048) != ((*offset + record.len() - 1) / 2048) {
            *offset = (*offset / 2048 + 1) * 2048;
        }
        directory[*offset..*offset + record.len()].copy_from_slice(record);
        *offset += record.len();
    }

    fn synthetic_iso(raw_mode: Option<u8>) -> Vec<u8> {
        const SYSTEM_LBA: u32 = 20;
        const BOOT_LBA: u32 = 21;
        let mut sectors = vec![[0u8; 2048]; 24];
        let pvd = &mut sectors[16];
        pvd[0] = 1;
        pvd[1..6].copy_from_slice(b"CD001");
        pvd[6] = 1;
        pvd[128..132].copy_from_slice(&le_be16(2048));
        let root = record(&[0], 19, 2048, true);
        pvd[156..156 + root.len()].copy_from_slice(&root);

        let mut root_dir = [0u8; 2048];
        let mut cursor = 0;
        add_record(&mut root_dir, &record(&[0], 19, 2048, true), &mut cursor);
        add_record(&mut root_dir, &record(&[1], 19, 2048, true), &mut cursor);
        add_record(
            &mut root_dir,
            &record(b"SYSTEM.CNF;1", SYSTEM_LBA, 36, false),
            &mut cursor,
        );
        add_record(&mut root_dir, &record(b"BOOT", 22, 2048, true), &mut cursor);
        sectors[19] = root_dir;
        let cnf = b"BOOT2 = cdrom0:\\BOOT\\GAME.ELF;1\r\n";
        sectors[SYSTEM_LBA as usize][..cnf.len()].copy_from_slice(cnf);
        let mut boot_dir = [0u8; 2048];
        cursor = 0;
        add_record(&mut boot_dir, &record(&[0], 22, 2048, true), &mut cursor);
        add_record(&mut boot_dir, &record(&[1], 19, 2048, true), &mut cursor);
        add_record(
            &mut boot_dir,
            &record(b"GAME.ELF;1", BOOT_LBA, TEST_BOOT.len() as u32, false),
            &mut cursor,
        );
        sectors[22] = boot_dir;
        sectors[BOOT_LBA as usize][..TEST_BOOT.len()].copy_from_slice(TEST_BOOT);
        let Some(mode) = raw_mode else {
            return sectors.into_iter().flatten().collect();
        };
        let mut image = Vec::with_capacity(sectors.len() * RAW_SECTOR as usize);
        for (lba, sector) in sectors.iter().enumerate() {
            let mut header = [0u8; 24];
            header[..12].copy_from_slice(&RAW_SYNC);
            header[12] = (lba / 75 / 60) as u8;
            header[13] = ((lba / 75) % 60) as u8;
            header[14] = (lba % 75) as u8;
            header[15] = mode;
            if mode == 2 {
                header[16..20].copy_from_slice(&[0, 0, 8, 0]);
                header[20..24].copy_from_slice(&[0, 0, 8, 0]);
            }
            let header_size = if mode == 2 { 24 } else { 16 };
            image.extend_from_slice(&header[..header_size]);
            image.extend_from_slice(sector);
            let trailer_size = if mode == 2 { 280 } else { 288 };
            image.extend(std::iter::repeat_n(0, trailer_size));
        }
        image
    }

    fn definition() -> VersionDefinition {
        VersionDefinition {
            game_id: "synthetic".to_owned(),
            id: "fixture".to_owned(),
            label: "Synthetic fixture".to_owned(),
            boot_name: "BOOT\\GAME.ELF".to_owned(),
            boot_sha1: format!("{:x}", Sha1::digest(TEST_BOOT)),
        }
    }

    #[test]
    fn identifies_nested_boot_path_from_plain_iso_and_raw_mode2() {
        for raw_mode in [None, Some(1), Some(2)] {
            let directory = tempdir().unwrap();
            let path = directory.path().join(match raw_mode {
                Some(1) => "renamed-mode1.BIN",
                Some(2) => "renamed-mode2.IMG",
                _ => "anything.iso",
            });
            fs::write(&path, synthetic_iso(raw_mode)).unwrap();
            let definitions = vec![definition()];
            assert_eq!(read_boot_executable(&path).unwrap().0, "BOOT\\GAME.ELF");
            let (sender, receiver) = mpsc::channel();
            scan_folder(directory.path(), &definitions, sender);
            let mut found = false;
            let mut finished = false;
            let mut errors = Vec::new();
            for event in receiver {
                match event {
                    ScanEvent::Found(version) => {
                        found |= version.game_id == "synthetic" && version.version_id == "fixture";
                    }
                    ScanEvent::Finished {
                        inspected,
                        errors: scan_errors,
                    } => {
                        assert_eq!(inspected, 1);
                        errors = scan_errors;
                        finished = true;
                    }
                }
            }
            assert!(found, "fixture scan errors: {errors:?}");
            assert!(finished);
            assert_eq!(
                read_supported_boot_executable(&path, &definitions[0]).unwrap(),
                TEST_BOOT
            );
        }
    }

    #[test]
    fn rejects_changed_boot_bytes_and_path_traversal_through_file() {
        let directory = tempdir().unwrap();
        let path = directory.path().join("fixture.iso");
        fs::write(&path, synthetic_iso(None)).unwrap();
        let mut wrong = definition();
        wrong.boot_sha1 = "00".repeat(20);
        assert!(read_supported_boot_executable(&path, &wrong).is_err());
        let mut image = DiscImage::open(&path).unwrap();
        assert!(image.file("SYSTEM.CNF/CHILD").is_err());
    }

    #[test]
    fn validates_extracted_boot_executable_against_the_version_hash() {
        let directory = tempdir().unwrap();
        let executable = directory.path().join("boot.elf");
        fs::write(&executable, TEST_BOOT).unwrap();
        assert!(boot_executable_matches(&executable, &definition()));

        fs::write(&executable, b"changed boot executable").unwrap();
        assert!(!boot_executable_matches(&executable, &definition()));
    }

    #[test]
    fn scan_reports_missing_folder_as_a_folder_error() {
        let directory = tempdir().unwrap();
        let missing = directory.path().join("does-not-exist");
        let (sender, receiver) = mpsc::channel();
        scan_folder(&missing, &[], sender);
        match receiver.recv().unwrap() {
            ScanEvent::Finished { inspected, errors } => {
                assert_eq!(inspected, 0);
                assert_eq!(errors.len(), 1);
                assert!(errors[0].contains("does-not-exist"));
            }
            ScanEvent::Found(_) => panic!("missing folder cannot yield a match"),
        }
    }
}
