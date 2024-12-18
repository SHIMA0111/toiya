use std::fs::create_dir;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    let build_dir = format!("{}/build", &manifest_dir);
    if !PathBuf::from(&build_dir).exists() {
        create_dir(&build_dir).unwrap();
    }

    Command::new("cmake")
        .current_dir(&build_dir)
        .arg(&manifest_dir)
        .status()
        .expect("Failed to run cmake");

    Command::new("make")
        .current_dir(&build_dir)
        .status()
        .expect("Failed to run make");

    let lib_name = if cfg!(target_os = "macos") {
        "libtoiya.dylib"
    } else if cfg!(target_os = "linux") {
        "libtoiya.so"
    } else {
        "toiya.dll"
    };

    let source_dir = PathBuf::from(&build_dir).join(format!("lib/{}", lib_name));
    let dest_dir = PathBuf::from(&out_dir).join(lib_name);

    std::fs::copy(source_dir, dest_dir).expect("Failed to copy lib to OUT_DIR");

    println!("cargo:rustc-link-search=native={}", out_dir);
    println!("cargo:rustc-link-lib=dylib=toiya");

    println!("cargo:rerun-if-changed=src/toiya-hyperapi/src/reader_sample.cpp");
    println!("cargo:rerun-if-changed=src/toiya-hyperapi/src/reader_sample.hpp");
    println!("cargo:rerun-if-changed=src/toiya-hyperapi/CMakeLists.txt");
}