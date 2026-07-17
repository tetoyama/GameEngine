// =======================================================================
//
// CommandSchema.cpp
//
// =======================================================================
#include "CommandSchema.h"

#include <cmath>
#include <exception>
#include <string>

namespace agentos {

namespace {

// float型でも整数値ならintegerとして許容する（LLM/JSON側の型揺れを吸収する）。
bool IsIntegral(const Json& value) {
	if (value.is_number_integer() || value.is_number_unsigned()) {
		return true;
	}
	if (value.is_number_float()) {
		double raw = value.get<double>();
		return raw == std::floor(raw);
	}
	return false;
}

bool CheckPrimitiveType(const Json& value, const std::string& type) {
	if (type == "integer") return IsIntegral(value);
	if (type == "number")  return value.is_number();
	if (type == "string")  return value.is_string();
	if (type == "boolean") return value.is_boolean();
	if (type == "array")   return value.is_array();
	if (type == "object")  return value.is_object();
	return false; // 未知の型指定は常に不一致扱い
}

} // namespace

Result SchemaValidator::Validate(const Json& arguments, const Json& argumentSchema) {
	try {
		if (!arguments.is_object()) {
			return Result::Fail("arguments must be a JSON object");
		}
		const bool hasSchema = argumentSchema.is_object();
		if (!argumentSchema.is_null() && !hasSchema) {
			return Result::Fail("argumentSchema must be a JSON object");
		}

		// 未知フィールドの拒否
		for (auto it = arguments.begin(); it != arguments.end(); ++it) {
			if (!hasSchema || !argumentSchema.contains(it.key())) {
				return Result::Fail("unknown field: " + it.key());
			}
		}

		if (!hasSchema) {
			return Result::Ok(); // スキーマ未指定かつargumentsも空
		}

		for (auto schemaIt = argumentSchema.begin(); schemaIt != argumentSchema.end(); ++schemaIt) {
			const std::string& fieldName = schemaIt.key();
			const Json& spec = schemaIt.value();
			const bool required = spec.value("required", false);
			const bool present = arguments.contains(fieldName);

			if (!present) {
				if (required) {
					return Result::Fail("missing required field: " + fieldName);
				}
				continue;
			}

			const Json& value = arguments.at(fieldName);
			const std::string type = spec.value("type", std::string());

			if (!type.empty() && !CheckPrimitiveType(value, type)) {
				return Result::Fail("type mismatch for field '" + fieldName + "': expected " + type);
			}

			if (type == "integer" || type == "number") {
				const double numeric = value.get<double>();
				if (spec.contains("min") && numeric < spec.at("min").get<double>()) {
					return Result::Fail("field '" + fieldName + "' is below minimum");
				}
				if (spec.contains("max") && numeric > spec.at("max").get<double>()) {
					return Result::Fail("field '" + fieldName + "' is above maximum");
				}
			}

			if (type == "string" && spec.contains("maxLength")) {
				const std::size_t maxLength = spec.at("maxLength").get<std::size_t>();
				if (value.get<std::string>().size() > maxLength) {
					return Result::Fail("field '" + fieldName + "' exceeds maxLength");
				}
			}

			if (type == "array" && spec.contains("itemType")) {
				const std::string itemType = spec.at("itemType").get<std::string>();
				for (const auto& item : value) {
					if (!CheckPrimitiveType(item, itemType)) {
						return Result::Fail(
							"field '" + fieldName + "' has an item with wrong type, expected " + itemType);
					}
				}
			}

			if (spec.contains("enum")) {
				const Json& allowed = spec.at("enum");
				bool found = false;
				for (const auto& candidate : allowed) {
					if (candidate == value) {
						found = true;
						break;
					}
				}
				if (!found) {
					return Result::Fail("field '" + fieldName + "' is not one of the allowed enum values");
				}
			}
		}

		return Result::Ok();
	} catch (const std::exception& e) {
		// nlohmann jsonの型変換例外等はAPI境界でResultへ変換する
		return Result::Fail(std::string("schema validation error: ") + e.what());
	}
}

} // namespace agentos
