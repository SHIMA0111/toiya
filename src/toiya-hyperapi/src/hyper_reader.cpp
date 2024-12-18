#include "hyper_reader.hpp"

#include <nanoarrow/nanoarrow.hpp>
#include <hyperapi/hyperapi.hpp>

namespace {
    struct ToiyaHyperApiError {
        static constexpr auto APPEND_FAILED = "Failed to append ";
        static constexpr auto BUFFER_FAILED = "Could not append buffer ";
        static constexpr auto SCHEMA_FAILED = "Failed to set schema ";
    };

    [[noreturn]] inline void throw_toiya_hyper_api_error(const char* format, const char* type) {
        throw std::runtime_error(std::string(format) + type);
    }
}


class HyperToArrow {
public:
    explicit HyperToArrow(ArrowArray* array) : arrow_array(array) {}
    virtual ~HyperToArrow() = default;

    auto Load(const hyperapi::Value& value) -> void {
        if (value.isNull()) {
            handleNull();
            return;
        }
        LoadValue(value);
    }

protected:
    auto GetMutableArray() { return arrow_array;};
    virtual void LoadValue(const hyperapi::Value& value) = 0;
    void handleNull() {
        if (ArrowArrayAppendNull(GetMutableArray(), 1)) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "null");
        }
    }

private:
    ArrowArray* arrow_array;
};

template <typename T> class IntegerHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;
protected:
    void LoadValue(const hyperapi::Value& value) override {
        if (ArrowArrayAppendInt(GetMutableArray(), value.get<T>())) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "integer");
        }
    }
};

class OidHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        if (ArrowArrayAppendUInt(GetMutableArray(), value.get<uint32_t>())) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "oid");
        }
    }
};

template <typename T> class FloatHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        if (ArrowArrayAppendDouble(GetMutableArray(), value.get<T>())) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "float");
        }
    }
};

class BooleanHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        if (ArrowArrayAppendInt(GetMutableArray(), value.get<bool>())) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "boolean");
        }
    }
};

class BytesHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        if (ArrowArrayAppendBytes(GetMutableArray(),
                                  {{value.get<hyperapi::ByteSpan>().data},
                                   static_cast<int64_t>(value.get<hyperapi::ByteSpan>().size)})) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "bytes");
        }
    }
};

class StringHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        const auto strviwe = value.get<std::string_view>();
        const ArrowStringView arrow_string_view {
            strviwe.data(), static_cast<int64_t>(strviwe.size())
        };

        if (ArrowArrayAppendString(GetMutableArray(), arrow_string_view)) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "string");
        }
    }
};

class DateHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        // tableau treats the Date as Julian Day (in proleptic Gregorian calendar's 11-24-4714BC (equals Julian calendar's 1-1-4713BC)).
        // Please see: https://www.postgresql.jp/docs/9.4/datetime-units-history.html
        constexpr int32_t julian_days_of_unix_epoch = 2440588;
        const auto hyper_date = value.get<hyperapi::Date>();

        // From https://community.tableau.com/s/question/0D54T00000C5Qd1SAF/tableau-hyper-api-datetime-int64-format,
        // tableau hyper file stores int32 of Julian Day.
        const auto tableau_date_raw = static_cast<int32_t>(hyper_date.getRaw());

        const auto epoch_day = tableau_date_raw - julian_days_of_unix_epoch;

        // Update data buffer (data store)
        ArrowBuffer* date_buffer = ArrowArrayBuffer(GetMutableArray(), 1);
        if (ArrowBufferAppendInt32(date_buffer, epoch_day)) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "date32");
        }

        // Update bitmap correctly
        ArrowBitmap* validity_bitmap = ArrowArrayValidityBitmap(GetMutableArray());
        if (ArrowBitmapAppend(validity_bitmap, true, 1)) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::BUFFER_FAILED, "date32");
        }

        // Increment the length because the data_buffer/bitmap updated
        GetMutableArray()->length++;
    }
};

template <bool TimeZone> class DateTimeHyperToArrow : public HyperToArrow {
    using HyperToArrow::HyperToArrow;

    void LoadValue(const hyperapi::Value& value) override {
        // Timezone is granted, treat as OffsetTimestamp correctly
        using timestamp_type = std::conditional_t<TimeZone, hyperapi::OffsetTimestamp, hyperapi::Timestamp>;

        const auto hyper_ts = value.get<timestamp_type>();

        // int64 can expose around 292271 years' usec at maximum. Therefore, julian usec is safe.
        // julian year is around 7000 years.
        constexpr int64_t julian_usec_of_unix_epoch = 2440588LL * 24 * 60 * 60 * 1000 * 1000;
        const auto tableau_usec_raw = static_cast<int64_t>(hyper_ts.getRaw());
        const auto epoch_usec = tableau_usec_raw - julian_usec_of_unix_epoch;

        ArrowBuffer* datetime_buffer = ArrowArrayBuffer(GetMutableArray(), 1);
        if (ArrowBufferAppendInt64(datetime_buffer, epoch_usec)) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::APPEND_FAILED, "timestamp64");
        }

        ArrowBitmap* validity_bitmap = ArrowArrayValidityBitmap(GetMutableArray());
        if (ArrowBitmapAppend(validity_bitmap, true, 1)) {
            throw_toiya_hyper_api_error(ToiyaHyperApiError::BUFFER_FAILED, "timestamp64");
        }
        GetMutableArray()->length++;
    }
};
