//===----------------------------------------------------------------------===//
//                         DuckDB
//
// functions/deltatable_scan.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "delta_utils.hpp"
#include "duckdb/common/multi_file_reader.hpp"

namespace duckdb {

struct DeltaFileMetaData {
    idx_t delta_snapshot_version;
    idx_t file_number;
};

//! The DeltaTableSnapshot implements the MultiFileList API to allow injecting it into the regular DuckDB parquet scan
struct DeltaTableSnapshot : public MultiFileList {
    explicit DeltaTableSnapshot(const string &path);

    //! MultiFileList API
    string GetFile(idx_t i) override;
    bool ComplexFilterPushdown(ClientContext &context, const MultiFileReaderOptions &options, LogicalGet &get,
                               vector<unique_ptr<Expression>> &filters) override;
    void Bind(vector<LogicalType> &return_types, vector<string> &names);


    //! Demo function for testing; to be replaced and potentially baseclassed in duckdb (for usage in statistics, progress
    //! calculation etc)
    DeltaFileMetaData GetFileMetadata(const string &path) {
        return metadata[path];
    };

protected:
    // TODO: How to guarantee we only call this after the filter pushdown?
    void InitializeFiles();

protected:
    //! Table Info
    string path;
    idx_t version;

    //! Delta Kernel Structures
    const ffi::SnapshotHandle *snapshot;
    const ffi::ExternEngineInterfaceHandle *table_client;
    UniqueKernelPointer <ffi::KernelScanFileIterator> files;

    //! Names
    vector<string> names;

    //! Metadata map for files
    case_insensitive_map_t<DeltaFileMetaData> metadata;

    //! Current file list resolution state
    bool initialized = false;
    vector<string> resolved_files;
    TableFilterSet table_filters;
};

struct DeltaMultiFileReader : public MultiFileReader {
    static unique_ptr<MultiFileReader> CreateInstance();

    //! Override the regular InitializeFiles; instead of producing a MultiFileList, we want a DeltaTableSnapshot
    void InitializeFiles(ClientContext &context, const Value &input, const string &name,
                                          FileGlobOptions options) override;

    //! Override the regular parquet bind using the MultiFileReader Bind. The bind from these are what DuckDB's file
    //! readers will try read
    bool Bind(vector<LogicalType> &return_types, vector<string> &names, MultiFileReaderBindData &bind_data) override;

    //! Override the Options bind. (could be superfluous?) can Bind and BindOptions be the same call?
    void BindOptions(vector<LogicalType> &return_types, vector<string> &names, MultiFileReaderBindData& bind_data) override;

    void FinalizeBind(const MultiFileReaderBindData &options, const string &filename, const vector<string> &local_names,
                                       const vector<LogicalType> &global_types, const vector<string> &global_names,
                                       const vector<column_t> &global_column_ids, MultiFileReaderData &reader_data,
                                       ClientContext &context) override;
    //! Override the FinalizeChunk method
    void FinalizeChunk(ClientContext &context, const MultiFileReaderBindData &bind_data,
                       const MultiFileReaderData &reader_data, DataChunk &chunk, const string &filename) override;

    //! Override the ParseOption call to parse delta_scan specific options
    bool ParseOption(const string &key, const Value &val, ClientContext &context) override;
};

struct DeltaMultiFileReaderBindData : public CustomMultiFileReaderBindData {

    DeltaMultiFileReaderBindData(DeltaTableSnapshot& delta_table_snapshot);

    //! The current MultiFileList
    DeltaTableSnapshot& current_snapshot;

    //! Bind data for demo generated column option
    idx_t file_number_column_idx = DConstants::INVALID_INDEX;
};

} // namespace duckdb
