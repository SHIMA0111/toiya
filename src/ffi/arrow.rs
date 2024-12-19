use std::ffi::CString;
use arrow::array::{RecordBatch, RecordBatchReader};
use arrow::compute::concat_batches;
use arrow::ffi_stream::{ArrowArrayStreamReader, FFI_ArrowArrayStream};

#[repr(C)]
struct CResult {
    data: *mut std::ffi::c_void,
    name: *const std::ffi::c_char,
    release: unsafe extern "C" fn(data: *mut std::ffi::c_void),
}


extern "C"{
    fn read_from_hyper_query_c(
        path: *const std::ffi::c_char,
        query: *const std::ffi::c_char,
        chunk_size: usize,
    ) -> CResult;
}


pub fn read_from_hyper(path: &str, chunk_size: usize) -> Result<RecordBatch, Box<dyn std::error::Error>> {
    unsafe {
        let path = CString::new(path).unwrap();
        let query = CString::new("SELECT * FROM spaceship").unwrap();
        let result = read_from_hyper_query_c(path.as_ptr(), query.as_ptr(), chunk_size);

        if result.data.is_null() {
            return Err("Failed to read from hyper".into());
        }

        let stream_ptr = result.data as *mut FFI_ArrowArrayStream;
        let mut stream_reader = ArrowArrayStreamReader::from_raw(stream_ptr)
            .expect("Failed to create ArrowArrayStreamReader");

        let schema = stream_reader.schema();

        let mut batches: Vec<RecordBatch> = Vec::new();
        while let Some(batch) = stream_reader.next() {
            let record_batch = batch.expect("Failed to read record batch");
            batches.push(record_batch);
        }

        let combined_batch = concat_batches(&schema, &batches).expect("Failed to concat batches");
        println!("Columns: {}, Rows: {}", combined_batch.num_columns(), combined_batch.num_rows());
        println!("{:?}", combined_batch);

        (result.release)(result.data);

        Ok(combined_batch)
    }
}