use std::pin::Pin;
use fcb_core::{FcbWriter, read_cityjson, CJType, CJTypeKind};
use fcb_core::header_writer::HeaderWriterOptions;
use fcb_core::attribute::{AttributeSchema, AttributeSchemaMethods};
use cjseq2::CityJSONFeature;
use std::fs::File;
use std::io::{Write, BufWriter};

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        type FcbWriterHandle;

        fn create_fcb_writer() -> Box<FcbWriterHandle>;
        fn write_metadata(
            writer: Pin<&mut FcbWriterHandle>,
            json_str: &str,
            path: &str,
            feature_count: u64,
            attribute_schema_json: &str,
        );
        fn write_feature(
            writer: Pin<&mut FcbWriterHandle>,
            json_str: &str,
        );
        fn close_file(writer: Pin<&mut FcbWriterHandle>);
    }
}

pub struct FcbWriterHandle {
    writer: Option<Box<FcbWriter<'static>>>,
    output_path: Option<String>,
    temp_metadata_file: Option<String>,
    features: Vec<Box<CityJSONFeature>>,  // Store features to keep them alive with 'static lifetime
}

impl FcbWriterHandle {
    fn new() -> Self {
        Self {
            writer: None,
            output_path: None,
            temp_metadata_file: None,
            features: Vec::new(),
        }
    }

    fn write_metadata(
        &mut self,
        json_str: &str,
        path: &str,
        feature_count: u64,
        attribute_schema_json: &str,
    ) -> std::result::Result<(), String> {
        // Store the output path - we'll create the file in close() when writing
        self.output_path = Some(path.to_string());

        // Write metadata JSON to temporary file for fcb_core to read
        let temp_path = format!("{}.metadata.json", path);
        let mut temp_file = File::create(&temp_path)
            .map_err(|e| format!("Failed to create temp file: {}", e))?;
        temp_file.write_all(json_str.as_bytes())
            .map_err(|e| format!("Failed to write temp file: {}", e))?;
        drop(temp_file);
        
        self.temp_metadata_file = Some(temp_path.clone());

        // Read CityJSON from the temp file (Normal type for metadata)
        let cj_type = read_cityjson(&temp_path, CJTypeKind::Normal)
            .map_err(|e| format!("Failed to read CityJSON: {}", e))?;

        match cj_type {
            CJType::Normal(cj) => {
                // Build AttributeSchema upfront from the provided JSON to avoid
                // per-feature "own_schema" overhead. This improves compression.
                let mut attr_schema = AttributeSchema::new();
                if !attribute_schema_json.is_empty() {
                    if let Ok(schema_json) = serde_json::from_str::<serde_json::Value>(attribute_schema_json) {
                        attr_schema.add_attributes(&schema_json);
                    }
                }

                // FcbWriter::new takes CityJSON by value
                // The lifetime parameter is for features, not the CityJSON in new()
                // IMPORTANT:
                // - Tyler requires an index, and fcb_core's reader treats files as "NoIndex"
                //   when header.features_count() == 0.
                // - Therefore we must set feature_count correctly in the header options.
                let header_options = HeaderWriterOptions {
                    feature_count,
                    ..Default::default()
                };
                let writer = FcbWriter::new(cj, Some(header_options), Some(attr_schema), None)
                    .map_err(|e| format!("Failed to create FcbWriter: {}", e))?;
                self.writer = Some(Box::new(writer));
            }
            _ => {
                return Err("Expected Normal CityJSON type for metadata".to_string());
            }
        }

        Ok(())
    }

    fn write_feature(&mut self, json_str: &str) -> std::result::Result<(), String> {
        // Parse the JSON string to extract the CityJSONFeature
        let feature: CityJSONFeature = serde_json::from_str(json_str)
            .map_err(|e| format!("Failed to parse CityJSONFeature from JSON: {}", e))?;
        
        // Store feature in a Box, then leak it to get 'static lifetime
        // This is necessary because FcbWriter::add_feature requires &'a CityJSONFeature
        // where 'a matches the FcbWriter lifetime ('static in our case)
        let feature_boxed = Box::new(feature);
        let feature_static: &'static CityJSONFeature = Box::leak(feature_boxed.clone());
        self.features.push(feature_boxed);
        
        if let Some(writer) = &mut self.writer {
            // writer is Box<FcbWriter<'static>>, so we can use 'static features
            writer.add_feature(feature_static)
                .map_err(|e| format!("Failed to add feature: {}", e))?;
        } else {
            return Err("Writer not initialized. Call write_metadata first.".to_string());
        }

        Ok(())
    }

    fn close(&mut self) -> std::result::Result<(), String> {
        if let Some(writer) = self.writer.take() {
            if let Some(path) = &self.output_path {
                // Create the output file with BufWriter for better performance
                let file = File::create(path)
                    .map_err(|e| format!("Failed to create FCB output file {}: {}", path, e))?;
                let mut buf_writer = BufWriter::new(file);
                
                // Write the complete FCB file (including index) using the writer
                writer.write(&mut buf_writer)
                    .map_err(|e| format!("Failed to write FCB file: {}", e))?;
                
                // Flush the buffer to ensure all data is written
                buf_writer.flush()
                    .map_err(|e| format!("Failed to flush FCB file buffer: {}", e))?;
                
                // Get the underlying file and sync it
                let file = buf_writer.into_inner()
                    .map_err(|e| format!("Failed to get inner file from BufWriter: {}", e))?;
                file.sync_all()
                    .map_err(|e| format!("Failed to sync FCB file: {}", e))?;
            } else {
                return Err("FCB output path is None - cannot write index".to_string());
            }
        } else {
            return Err("FCB writer is None - no features were written".to_string());
        }

        // Clean up temp metadata file
        if let Some(temp_path) = &self.temp_metadata_file {
            let _ = std::fs::remove_file(temp_path);
        }
        
        self.output_path = None;
        self.temp_metadata_file = None;
        self.features.clear();
        Ok(())
    }
}

fn create_fcb_writer() -> Box<FcbWriterHandle> {
    Box::new(FcbWriterHandle::new())
}

// Bridge implementation functions
// Use the Result type from the bridge - it's automatically available
// cxx converts std::result::Result<(), String> automatically
// Implementation functions
// For now using void return - we'll add proper error handling later
// Errors will panic for now, which C++ can catch
fn write_metadata(writer: Pin<&mut FcbWriterHandle>, json_str: &str, path: &str, feature_count: u64, attribute_schema_json: &str) {
    writer.get_mut().write_metadata(json_str, path, feature_count, attribute_schema_json)
        .expect("FCB write_metadata failed");
}

fn write_feature(writer: Pin<&mut FcbWriterHandle>, json_str: &str) {
    writer.get_mut().write_feature(json_str)
        .expect("FCB write_feature failed");
}

fn close_file(writer: Pin<&mut FcbWriterHandle>) {
    writer.get_mut().close()
        .expect("FCB close failed");
}
