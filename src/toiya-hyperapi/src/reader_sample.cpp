#include "reader_sample.hpp"

#include <span>
#include <variant>
#include <vector>

#include <hyperapi/hyperapi.hpp>
#include <nanoarrow/nanoarrow.hpp>

namespace gsl {
    template <typename T> using owner = T;
}

class ReadHelper {
public:
    ReadHelper(struct ArrowArray* array) : array_(array) {}
    ReadHelper(const ReadHelper&) = delete;
    ReadHelper &operator=(ReadHelper &) = delete;
    ReadHelper(ReadHelper &&) = delete;
    ReadHelper &operator=(ReadHelper &&) = delete;

    virtual ~ReadHelper() = default;
    virtual auto Read(const hyperapi::Value &) -> void = 0;

protected:
    auto GetMutableArray() { return array_; }

private:
    struct ArrowArray* array_;
};


template <typename T> class IntegralReadHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }
        if (ArrowArrayAppendInt(GetMutableArray(), value.get<T>())) {
            throw std::runtime_error("ArrowAppendInt failed");
        }
    }
};

class OidReadHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }
        if (ArrowArrayAppendUInt(GetMutableArray(), value.get<uint32_t>())) {
            throw std::runtime_error("ArrowAppendUInt failed");
        }
    }
};

template <typename T> class FloatReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }
        if (ArrowArrayAppendDouble(GetMutableArray(), value.get<T>())) {
            throw std::runtime_error("ArrowAppendDouble failed");
        }
    }
};

class BooleanReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }
        if (ArrowArrayAppendInt(GetMutableArray(), value.get<bool>())) {
            throw std::runtime_error("ArrowAppendBool failed");
        }
    }
};

class BytesReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

        const auto bytes = value.get<hyperapi::ByteSpan>();

        if (ArrowArrayAppendBytes(GetMutableArray(),
                                  {{bytes.data}, static_cast<int64_t>(bytes.size)})) {
            throw std::runtime_error("ArrowAppendBytes failed");
        }
    }
};

class StringReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

#if defined(_WIN32) && defined(_MSC_VER)
        const auto strval = value.get<std::string>();
        const ArrowStringView arrow_string_view{strval.c_str(), static_cast<int64_t>(strval.size())};
#else
        const auto strval = value.get<std::string_view>();
        const ArrowStringView arrow_string_view{strval.data(), static_cast<int64_t>(strval.size())};
#endif
        if (ArrowArrayAppendString(GetMutableArray(), arrow_string_view)) {
            throw std::runtime_error("ArrowAppendString failed");
        }
    }
};

class DateReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

        // getRaw returns the Julian calendar so to convert it to Unix Date, the tableau_to_unix_days
        // has julian day of the unix day. (BC 4713/1/1 as 0 to AC 1970/1/1, 2440588)
        constexpr int32_t tableau_to_unix_days = 2440588;
        const auto hyper_date = value.get<hyperapi::Date>();
        const auto tableau_date = static_cast<uint32_t>(hyper_date.getRaw());

        // Tableau uses uint32
        if (tableau_date > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
            throw std::range_error("Date value out of range");
        }

        const auto raw_value = static_cast<int32_t>(tableau_date);
        const auto arrow_value = raw_value - tableau_to_unix_days;

        auto array = GetMutableArray();
        struct ArrowBuffer *date_buffer = ArrowArrayBuffer(array, 1);
        if (ArrowBufferAppendInt32(date_buffer, arrow_value)) {
            throw std::runtime_error("Failed to append date32 value");
        }

        struct ArrowBitmap* validity_bitmap = ArrowArrayValidityBitmap(array);
        if (ArrowBitmapAppend(validity_bitmap, true, 1)) {
            throw std::runtime_error("Could not append validity buffer for date32");
        }
        array -> length++;
    }
};

template <bool TZAware> class DateTimeReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

        using timestamp_t = typename std::conditional<TZAware, hyperapi::OffsetTimestamp, hyperapi::Timestamp>::type;
        const auto hyper_ts = value.get<timestamp_t>();

        // TODO: need some bounds /overflow checking
        // tableau uses uint64 but we have int64
        constexpr int64_t tableau_to_unix_usec = 2440588LL * 24 * 60 * 60 * 1000 * 1000;
        const auto raw_usec = static_cast<int64_t>(hyper_ts.getRaw());
        const auto arrow_value = raw_usec - tableau_to_unix_usec;

        auto array = GetMutableArray();
        struct ArrowBuffer* data_buffer = ArrowArrayBuffer(array, 1);
        if (ArrowBufferAppendInt64(data_buffer, arrow_value)) {
            throw std::runtime_error("Failed to append timestamp64 value");
        }

        struct ArrowBitmap* validity_bitmap = ArrowArrayValidityBitmap(array);
        if (ArrowBitmapAppend(validity_bitmap, true, 1)) {
            throw std::runtime_error("Could not append validity buffer for timestamp");
        }
        array -> length++;
    }
};

class TimeReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

        const auto time = value.get<hyperapi::Time>();
        const auto raw_value = time.getRaw();
        if (ArrowArrayAppendInt(GetMutableArray(), static_cast<int64_t>(raw_value))) {
            throw std::runtime_error("ArrowAppendInt failed");
        }
    }
};

class IntervalReaderHelper : public ReadHelper {
    using ReadHelper::ReadHelper;

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

        struct ArrowInterval arrow_interval = {};
        ArrowIntervalInit(&arrow_interval, NANOARROW_TYPE_INTERVAL_MONTH_DAY_NANO);
        const auto interval_value = value.get<hyperapi::Interval>();
        constexpr auto MonthsPerYear = 12;
        constexpr auto NsPerHour = 3'600'000'000'000LL;
        constexpr auto NsPerMin = 60'000'000'000LL;
        constexpr auto NsPerSec = 1'000'000'000LL;
        constexpr auto NsPerUsec = 1'000LL;

        arrow_interval.months = interval_value.getYears() * MonthsPerYear + interval_value.getMonths();
        arrow_interval.days = interval_value.getDays();
        arrow_interval.ns =
            interval_value.getHours() * NsPerHour +
            interval_value.getMinutes() * NsPerMin +
            interval_value.getSeconds() * NsPerSec +
            interval_value.getMicroseconds() * NsPerUsec;

        if (ArrowArrayAppendInterval(GetMutableArray(), &arrow_interval)) {
            throw std::runtime_error("ArrowAppendInterval failed");
        }
    }
};

class DecimalReaderHelper : public ReadHelper {
public:
    explicit DecimalReaderHelper(struct ArrowArray* array, int32_t precision, int32_t scale)
        : ReadHelper(array), precision_(precision), scale_(scale) {}

    auto Read(const hyperapi::Value& value) -> void override {
        if (value.isNull()) {
            if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
                throw std::runtime_error("ArrowAppendNull failed");
            }
            return;
        }

        constexpr int32_t bitwidth = 128;
        struct ArrowDecimal decimal = {};
        ArrowDecimalInit(&decimal, bitwidth, precision_, scale_);

        constexpr auto PrecisionLimit = 39; // of-by-one error in solution?
        if (precision_ >= PrecisionLimit) {
            throw std::range_error("Precision limit exceeded");
        }
        if (scale_ >= PrecisionLimit) {
            throw std::range_error("Scale limit exceeded");
        }

        const auto decimal_string = std::visit(
            [&value](auto P, auto S) -> std::string {
                if constexpr (S() <= P()) {
                    const auto decimal_value = value.get<hyperapi::Numeric<P(), S()>>();
                    auto value_string = decimal_value.toString();
                    std::erase(value_string, '.');
                    return value_string;
                }
                throw std::runtime_error("unreachable error");
            },
            to_integral_variant<PrecisionLimit>(precision_),
            to_integral_variant<PrecisionLimit>(scale_));

        const struct ArrowStringView sv {
            decimal_string.data(), static_cast<int64_t>(decimal_string.size())
        };

        if (ArrowDecimalSetDigits(&decimal, sv)) {
            throw std::runtime_error("Unable to convert tableau numeric to arrow decimal");
        }

        if (ArrowArrayAppendDecimal(GetMutableArray(), &decimal)) {
            throw std::runtime_error("Failed to append decimal value");
        }
    }

private:
    int32_t precision_;
    int32_t scale_;
};

static auto MakeReadHelper(const ArrowSchemaView* schema_view,
                           struct ArrowArray* array) -> std::unique_ptr<ReadHelper> {
    switch (schema_view -> type) {
        case NANOARROW_TYPE_INT16:
            return std::unique_ptr<ReadHelper>(new IntegralReadHelper<int16_t>(array));
        case NANOARROW_TYPE_INT32:
            return std::unique_ptr<ReadHelper>(new IntegralReadHelper<int32_t>(array));
        case NANOARROW_TYPE_INT64:
            return std::unique_ptr<ReadHelper>(new IntegralReadHelper<int64_t>(array));
        case NANOARROW_TYPE_UINT32:
            return std::unique_ptr<ReadHelper>(new OidReadHelper(array));
        case NANOARROW_TYPE_FLOAT:
            return std::unique_ptr<ReadHelper>(new FloatReaderHelper<float>(array));
        case NANOARROW_TYPE_DOUBLE:
            return std::unique_ptr<ReadHelper>(new FloatReaderHelper<double>(array));
        case NANOARROW_TYPE_LARGE_BINARY:
            return std::unique_ptr<ReadHelper>(new BytesReaderHelper(array));
        case NANOARROW_TYPE_LARGE_STRING:
            return std::unique_ptr<ReadHelper>(new StringReaderHelper(array));
        case NANOARROW_TYPE_BOOL:
            return std::unique_ptr<ReadHelper>(new BooleanReaderHelper(array));
        case NANOARROW_TYPE_DATE32:
            return std::unique_ptr<ReadHelper>(new DateReaderHelper(array));
        case NANOARROW_TYPE_TIMESTAMP: {
            if (strcmp("", schema_view -> timezone) != 0) {
                return std::unique_ptr<ReadHelper>(new DateTimeReaderHelper<true>(array));
            } else {
                return std::unique_ptr<ReadHelper>(new DateTimeReaderHelper<false>(array));
            }
        }
        case NANOARROW_TYPE_INTERVAL_MONTH_DAY_NANO:
            return std::unique_ptr<ReadHelper>(new IntervalReaderHelper(array));
        case NANOARROW_TYPE_TIME64:
            return std::unique_ptr<ReadHelper>(new TimeReaderHelper(array));
        case NANOARROW_TYPE_DECIMAL128: {
            const auto precision = schema_view -> decimal_precision;
            const auto scale = schema_view -> decimal_scale;

            return std::unique_ptr<ReadHelper>(new DecimalReaderHelper(array, precision, scale));
        }
        default:
            throw std::format_error("unknown arrow type provided");
    }
}

static auto GetArrowTypeFromHyper(const hyperapi::SqlType& sql_type) -> enum ArrowType {
    switch (sql_type.getTag()) {
        case hyperapi::TypeTag::SmallInt : return NANOARROW_TYPE_INT16;
        case hyperapi::TypeTag::Int : return NANOARROW_TYPE_INT32;
        case hyperapi::TypeTag::BigInt : return NANOARROW_TYPE_INT64;
        case hyperapi::TypeTag::Oid : return NANOARROW_TYPE_UINT32;
        case hyperapi::TypeTag::Float : return NANOARROW_TYPE_FLOAT;
        case hyperapi::TypeTag::Double : return NANOARROW_TYPE_DOUBLE;
        case hyperapi::TypeTag::Geography : case hyperapi::TypeTag::Bytes : return NANOARROW_TYPE_LARGE_BINARY;
        case hyperapi::TypeTag::Varchar : case hyperapi::TypeTag::Char : case hyperapi::TypeTag::Text : case hyperapi::TypeTag::Json : return NANOARROW_TYPE_LARGE_STRING;
        case hyperapi::TypeTag::Bool : return NANOARROW_TYPE_BOOL;
        case hyperapi::TypeTag::Date : return NANOARROW_TYPE_DATE32;
        case hyperapi::TypeTag::Timestamp : case hyperapi::TypeTag::TimestampTZ : return NANOARROW_TYPE_TIMESTAMP;
        case hyperapi::TypeTag::Interval : return NANOARROW_TYPE_INTERVAL_MONTH_DAY_NANO;
        case hyperapi::TypeTag::Time : return NANOARROW_TYPE_TIME64;
        default : throw std::format_error("unknown hyper type provided: " + sql_type.toString());
    }
}

static auto SetSchemaTypeFromHyperType(struct ArrowSchema* schema, const hyperapi::SqlType& sql_type) -> void {
    switch (sql_type.getTag()) {
        case hyperapi::TypeTag::TimestampTZ:
            if (ArrowSchemaSetTypeDateTime(schema, NANOARROW_TYPE_TIMESTAMP, NANOARROW_TIME_UNIT_MICRO, "UTC")) {
                throw std::runtime_error("ArrowSchemaSetDateTime failed for TimestampTZ type");
            }
            break;
        case hyperapi::TypeTag::Timestamp:
            if (ArrowSchemaSetTypeDateTime(schema, NANOARROW_TYPE_TIMESTAMP, NANOARROW_TIME_UNIT_MICRO, nullptr)) {
                throw std::runtime_error("ArrowSchemaSetDateTime failed for Timestamp type");
            }
            break;
        case hyperapi::TypeTag::Time:
            if (ArrowSchemaSetTypeDateTime(schema, NANOARROW_TYPE_TIME64, NANOARROW_TIME_UNIT_MICRO, nullptr)) {
                throw std::runtime_error("ArrowSchemaSetDateTime failed for Time type");
            }
            break;
        case hyperapi::TypeTag::Numeric: {
            const auto precision = sql_type.getPrecision();
            const auto scale = sql_type.getScale();
            if (ArrowSchemaSetTypeDecimal(schema,
                                          NANOARROW_TYPE_DECIMAL128,
                                          static_cast<int32_t>(precision),
                                          static_cast<int32_t>(scale))) {
                throw std::runtime_error("ArrowSchemaSetDecimal failed");
            }
            break;
        }
        default:
            const enum ArrowType arrow_type = GetArrowTypeFromHyper(sql_type);
            if (ArrowSchemaSetType(schema, arrow_type)) {
                throw std::runtime_error("ArrowSchemaSetType failed");
            }
    }
}

struct HyperResultIteratorPrivate {
    HyperResultIteratorPrivate(std::unique_ptr<hyperapi::Result> result,
                               std::unique_ptr<hyperapi::ChunkedResultIterator> iter)
                               : result_(std::move(result)), iter_(std::move(iter)) {}

    std::unique_ptr<hyperapi::Result> result_;
    std::unique_ptr<hyperapi::ChunkedResultIterator> iter_;
    struct ArrowError error_ {};
};

static auto ReleaseArrowStream(void *ptr) noexcept -> void {
    auto stream = static_cast<gsl::owner<ArrowArrayStream *>>(ptr);
    if (stream -> release != nullptr) {
        ArrowArrayStreamRelease(stream);
    }

    delete stream;
}

static const auto GetSchema = [](struct ArrowArrayStream* stream, struct ArrowSchema* out) noexcept {
    auto private_data = static_cast<HyperResultIteratorPrivate*>(stream -> private_data);

    const auto resultSchema = private_data -> result_ -> getSchema();

    nanoarrow::UniqueSchema schema{};
    ArrowSchemaInit(schema.get());

    if (ArrowSchemaSetTypeStruct(
        schema.get(), static_cast<int64_t>(resultSchema.getColumnCount()))) {
        ArrowErrorSetString(&private_data -> error_, "ArrowSchemaSetTypeStruct failed");

        return EINVAL;
    }

    const auto column_count = resultSchema.getColumnCount();
    std::unordered_map<std::string, size_t> name_counter;
    const std::span children = {schema -> children, static_cast<size_t>(schema -> n_children)};

    for (size_t i = 0; i < column_count; i++) {
        const auto column = resultSchema.getColumn(i);
        auto name = column.getName().getUnescaped();
        const auto &[elem, did_insert] = name_counter.emplace(name, 0);

        if (!did_insert) {
            name += "_" + std::to_string(elem -> second);
        }
        elem -> second++;

        if (ArrowSchemaSetName(children[i], name.c_str())) {
            ArrowErrorSetString(&private_data -> error_, "ArrowSchemaSetName failed");
            return EINVAL;
        }

        SetSchemaTypeFromHyperType(children[i], column.getType());
    }

    ArrowSchemaMove(schema.get(), out);

    return 0;
};

static const auto GetNext = [](struct ArrowArrayStream* stream, struct ArrowArray* out) noexcept {
    auto private_data = static_cast<HyperResultIteratorPrivate*>(stream -> private_data);

    auto end = hyperapi::ChunkedResultIterator{*private_data -> result_, hyperapi::IteratorEndTag{}};

    if (*private_data -> iter_ == end) {
        return 0;
    }

    nanoarrow::UniqueSchema schema{};
    if (int errcode = GetSchema(stream, schema.get())) {
        return errcode;
    }

    const auto column_count = static_cast<size_t>(schema -> n_children);
    nanoarrow::UniqueArray array{};
    if (ArrowArrayInitFromSchema(array.get(), schema.get(), nullptr)) {
        ArrowErrorSetString(&private_data -> error_, "ArrowArrayInitFromSchema failed");
        return EINVAL;
    }

    // TODO: we might want to move the vector of ReadHelpers to the private_data
    // rather than doing on each loop iteration here.
    std::vector<std::unique_ptr<ReadHelper>> read_helpers {column_count};
    const std::span schema_children{schema -> children, static_cast<size_t>(schema -> n_children)};
    const std::span array_children{array -> children, static_cast<size_t>(array -> n_children)};

    for (size_t i = 0; i < column_count; i++) {
        struct ArrowSchemaView schema_view {};
        if (ArrowSchemaViewInit(&schema_view, schema_children[i], nullptr)) {
            ArrowErrorSetString(&private_data -> error_, "ArrowSchemaViewInit failed");
            return EINVAL;
        }

        auto read_helper = MakeReadHelper(&schema_view, array_children[i]);
        read_helpers[i] = std::move(read_helper);
    }

    if (ArrowArrayStartAppending(array.get())) {
        ArrowErrorSetString(&private_data -> error_, "ArrowArrayStartAppending failed");
        return EINVAL;
    }

    for (const auto& row : **private_data -> iter_) {
        size_t column_idx = 0;
        for (const auto& value : row) {
            const auto &read_helper = read_helpers[column_idx];
            read_helper->Read(value);
            column_idx++;
        }

        if (ArrowArrayFinishElement(array.get())) {
            ArrowErrorSetString(&private_data -> error_, "ArrowArrayFinishElement failed");
            return EINVAL;
        }
    }
    ++(*private_data->iter_);

    if (ArrowArrayFinishBuildingDefault(array.get(), nullptr)) {
        ArrowErrorSetString(&private_data -> error_, "ArrowArrayFinishBuildingDefault failed");
        return EINVAL;
    }

    ArrowArrayMove(array.get(), out);

    return 0;
};

auto read_from_hyper_query(const std::string& path,
                           const std::string& query,
                           std::unordered_map<std::string, std::string>&& process_params,
                           size_t chunk_size)-> Result {
    if (!process_params.count("log_config")) {
        process_params["log_config"] = "";
    } else {
        process_params.erase("log_config");
    }

    if (!process_params.count("default_database_version"))
        process_params["default_database_version"] = "2";

    const hyperapi::HyperProcess hyper{
        hyperapi::Telemetry::DoNotSendUsageDataToTableau,
        "",
        process_params};
    hyperapi::Connection connection(hyper.getEndpoint(), path);

    if (chunk_size) {
        hyper_set_chunked_mode(hyperapi::internal::getHandle(connection), true);
        hyper_set_chunk_size(hyperapi::internal::getHandle(connection), chunk_size);
    }

    auto hyperResult = std::make_unique<hyperapi::Result>(connection.executeQuery(query));

    auto iter = std::make_unique<hyperapi::ChunkedResultIterator>(*hyperResult, hyperapi::IteratorBeginTag{});

    auto private_data = gsl::owner<HyperResultIteratorPrivate*>(
        new HyperResultIteratorPrivate{std::move(hyperResult), std::move(iter)});

    auto stream = gsl::owner<struct ArrowArrayStream*>(new struct ArrowArrayStream);
    stream -> private_data = private_data;
    stream -> get_next = GetNext;
    stream -> get_schema = GetSchema;
    stream -> get_last_error = [](struct ArrowArrayStream* stream) {
        auto private_data = static_cast<HyperResultIteratorPrivate*>(stream->private_data);
        return static_cast<const char*>(private_data -> error_.message);
    };

    stream -> release = [](struct ArrowArrayStream* stream) {
        auto private_data = static_cast<gsl::owner<HyperResultIteratorPrivate*>>(
            stream -> private_data);
        delete private_data;
        stream -> release = nullptr;
    };

    Result result{stream, "arrow_array_stream", &ReleaseArrowStream};
    return result;
}
