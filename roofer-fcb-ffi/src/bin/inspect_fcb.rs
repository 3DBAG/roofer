use std::fs::File;
use std::io::BufReader;

use fcb_core::FcbReader;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args()
        .nth(1)
        .expect("usage: inspect_fcb <path-to-fcb>");

    let file = File::open(&path)?;
    let reader = BufReader::new(file);
    let fcb = FcbReader::open(reader)?;
    let header = fcb.header();

    println!("file: {}", path);
    println!("features_count: {}", header.features_count());
    println!("index_node_size: {}", header.index_node_size());
    println!("has_attribute_index: {}", header.attribute_index().is_some());

    Ok(())
}

