#ifndef __ACTISENSE_SDK_TESTS_SUPPORT_MINI_JSON_HPP
#define __ACTISENSE_SDK_TESTS_SUPPORT_MINI_JSON_HPP

/*==============================================================================
\file       mini_json.hpp
\author     (Created) Claude Code
\date       (Created) 06/08/2026
\brief      Minimal JSON reader for test-support config files.
\details    The integration rig description (rig config) and the PGN manifest
			are small machine-generated JSON documents. The SDK deliberately
			carries no JSON dependency, so this header provides a tiny
			recursive-descent reader sufficient for those documents: objects,
			arrays, strings (with escapes), numbers, booleans and null. It is
			test support only — nothing under src/ may include it.

			Numbers are held as double, which is exact for every integer the
			manifest can carry (PGNs, byte counts, indices are all far below
			2^53).

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			namespace MiniJson
			{
				/* Definitions ---------------------------------------------------------- */

				/**************************************************************************/ /**
				 \brief      A parsed JSON value (null / bool / number / string /
							 array / object).
				 *******************************************************************************/
				class Value
				{
				public:
					using Array = std::vector<Value>;
					using Object = std::map<std::string, Value>;
					using Storage =
						std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

					Value() : storage_(nullptr) {}
					explicit Value(Storage storage) : storage_(std::move(storage)) {}

					[[nodiscard]] bool isNull() const
					{
						return std::holds_alternative<std::nullptr_t>(storage_);
					}
					[[nodiscard]] bool isBool() const
					{
						return std::holds_alternative<bool>(storage_);
					}
					[[nodiscard]] bool isNumber() const
					{
						return std::holds_alternative<double>(storage_);
					}
					[[nodiscard]] bool isString() const
					{
						return std::holds_alternative<std::string>(storage_);
					}
					[[nodiscard]] bool isArray() const
					{
						return std::holds_alternative<Array>(storage_);
					}
					[[nodiscard]] bool isObject() const
					{
						return std::holds_alternative<Object>(storage_);
					}

					[[nodiscard]] std::optional<bool> asBool() const
					{
						if (const bool* b = std::get_if<bool>(&storage_)) {
							return *b;
						}
						return std::nullopt;
					}

					[[nodiscard]] std::optional<double> asNumber() const
					{
						if (const double* d = std::get_if<double>(&storage_)) {
							return *d;
						}
						return std::nullopt;
					}

					/**************************************************************************/ /**
					 \brief      Number as a non-negative integer (uint32), if it is one.
					 \details    Rejects negatives, fractions and anything above
								 uint32 range — config fields this reader serves are
								 all small non-negative integers.
					 *******************************************************************************/
					[[nodiscard]] std::optional<uint32_t> asUint32() const
					{
						const auto num = asNumber();
						if (!num.has_value() || *num < 0.0 || *num > 4294967295.0) {
							return std::nullopt;
						}
						const auto integral = static_cast<uint64_t>(*num);
						if (static_cast<double>(integral) != *num) {
							return std::nullopt;
						}
						return static_cast<uint32_t>(integral);
					}

					[[nodiscard]] std::optional<std::string_view> asString() const
					{
						if (const std::string* s = std::get_if<std::string>(&storage_)) {
							return std::string_view(*s);
						}
						return std::nullopt;
					}

					[[nodiscard]] const Array* asArray() const
					{
						return std::get_if<Array>(&storage_);
					}

					[[nodiscard]] const Object* asObject() const
					{
						return std::get_if<Object>(&storage_);
					}

					/**************************************************************************/ /**
					 \brief      Object member lookup; nullptr when absent or when this
								 value is not an object.
					 *******************************************************************************/
					[[nodiscard]] const Value* find(std::string_view key) const
					{
						const Object* object = asObject();
						if (object == nullptr) {
							return nullptr;
						}
						const auto it = object->find(std::string(key));
						return (it == object->end()) ? nullptr : &it->second;
					}

				private:
					Storage storage_;
				};

				namespace detail
				{
					class Parser
					{
					public:
						Parser(std::string_view text, std::string& error)
							: text_(text), error_(error)
						{
						}

						[[nodiscard]] std::optional<Value> run()
						{
							skipWhitespace();
							auto value = parseValue(0);
							if (!value.has_value()) {
								return std::nullopt;
							}
							skipWhitespace();
							if (pos_ != text_.size()) {
								return fail("trailing content after JSON document");
							}
							return value;
						}

					private:
						static constexpr std::size_t kMaxDepth = 64;

						std::string_view text_;
						std::string& error_;
						std::size_t pos_ = 0;

						[[nodiscard]] std::optional<Value> fail(const std::string& why)
						{
							if (error_.empty()) {
								error_ = why + " (offset " + std::to_string(pos_) + ")";
							}
							return std::nullopt;
						}

						void skipWhitespace()
						{
							while (pos_ < text_.size()) {
								const char c = text_[pos_];
								if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
									++pos_;
								} else {
									break;
								}
							}
						}

						[[nodiscard]] bool consume(char expected)
						{
							if (pos_ < text_.size() && text_[pos_] == expected) {
								++pos_;
								return true;
							}
							return false;
						}

						[[nodiscard]] bool consumeLiteral(std::string_view literal)
						{
							if (text_.substr(pos_, literal.size()) == literal) {
								pos_ += literal.size();
								return true;
							}
							return false;
						}

						[[nodiscard]] std::optional<Value> parseValue(std::size_t depth)
						{
							if (depth > kMaxDepth) {
								return fail("nesting too deep");
							}
							skipWhitespace();
							if (pos_ >= text_.size()) {
								return fail("unexpected end of document");
							}
							const char c = text_[pos_];
							if (c == '{') {
								return parseObject(depth);
							}
							if (c == '[') {
								return parseArray(depth);
							}
							if (c == '"') {
								auto str = parseString();
								if (!str.has_value()) {
									return std::nullopt;
								}
								return Value(Value::Storage(std::move(*str)));
							}
							if (consumeLiteral("true")) {
								return Value(Value::Storage(true));
							}
							if (consumeLiteral("false")) {
								return Value(Value::Storage(false));
							}
							if (consumeLiteral("null")) {
								return Value(Value::Storage(nullptr));
							}
							return parseNumber();
						}

						[[nodiscard]] std::optional<Value> parseObject(std::size_t depth)
						{
							if (!consume('{')) {
								return fail("expected '{'");
							}
							Value::Object object;
							skipWhitespace();
							if (consume('}')) {
								return Value(Value::Storage(std::move(object)));
							}
							while (true) {
								skipWhitespace();
								auto key = parseString();
								if (!key.has_value()) {
									return fail("expected object key string");
								}
								skipWhitespace();
								if (!consume(':')) {
									return fail("expected ':' after object key");
								}
								auto value = parseValue(depth + 1);
								if (!value.has_value()) {
									return std::nullopt;
								}
								object.emplace(std::move(*key), std::move(*value));
								skipWhitespace();
								if (consume(',')) {
									continue;
								}
								if (consume('}')) {
									return Value(Value::Storage(std::move(object)));
								}
								return fail("expected ',' or '}' in object");
							}
						}

						[[nodiscard]] std::optional<Value> parseArray(std::size_t depth)
						{
							if (!consume('[')) {
								return fail("expected '['");
							}
							Value::Array array;
							skipWhitespace();
							if (consume(']')) {
								return Value(Value::Storage(std::move(array)));
							}
							while (true) {
								auto value = parseValue(depth + 1);
								if (!value.has_value()) {
									return std::nullopt;
								}
								array.push_back(std::move(*value));
								skipWhitespace();
								if (consume(',')) {
									continue;
								}
								if (consume(']')) {
									return Value(Value::Storage(std::move(array)));
								}
								return fail("expected ',' or ']' in array");
							}
						}

						[[nodiscard]] std::optional<std::string> parseString()
						{
							if (!consume('"')) {
								error_ = "expected string";
								return std::nullopt;
							}
							std::string out;
							while (pos_ < text_.size()) {
								const char c = text_[pos_++];
								if (c == '"') {
									return out;
								}
								if (c != '\\') {
									out.push_back(c);
									continue;
								}
								if (pos_ >= text_.size()) {
									break;
								}
								const char esc = text_[pos_++];
								switch (esc) {
									case '"': out.push_back('"'); break;
									case '\\': out.push_back('\\'); break;
									case '/': out.push_back('/'); break;
									case 'b': out.push_back('\b'); break;
									case 'f': out.push_back('\f'); break;
									case 'n': out.push_back('\n'); break;
									case 'r': out.push_back('\r'); break;
									case 't': out.push_back('\t'); break;
									case 'u': {
										const auto unit = parseHex4();
										if (!unit.has_value()) {
											error_ = "bad \\u escape";
											return std::nullopt;
										}
										uint32_t codepoint = *unit;
										/* Combine a UTF-16 surrogate pair. A lone
										   surrogate would encode invalid UTF-8 that
										   could flow into report artifacts - reject. */
										if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
											error_ = "lone low surrogate";
											return std::nullopt;
										}
										if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
											if (!consumeLiteral("\\u")) {
												error_ = "lone high surrogate";
												return std::nullopt;
											}
											const auto low = parseHex4();
											if (!low.has_value() || *low < 0xDC00u ||
												*low > 0xDFFFu) {
												error_ = "bad surrogate pair";
												return std::nullopt;
											}
											codepoint = 0x10000u +
												((codepoint - 0xD800u) << 10) + (*low - 0xDC00u);
										}
										appendUtf8(out, codepoint);
										break;
									}
									default:
										error_ = "unknown escape";
										return std::nullopt;
								}
							}
							error_ = "unterminated string";
							return std::nullopt;
						}

						[[nodiscard]] std::optional<uint32_t> parseHex4()
						{
							if (pos_ + 4 > text_.size()) {
								return std::nullopt;
							}
							uint32_t value = 0;
							for (uint32_t i = 0; i < 4; ++i) {
								const char c = text_[pos_ + i];
								value <<= 4;
								if (c >= '0' && c <= '9') {
									value |= static_cast<uint32_t>(c - '0');
								} else if (c >= 'a' && c <= 'f') {
									value |= static_cast<uint32_t>(c - 'a' + 10);
								} else if (c >= 'A' && c <= 'F') {
									value |= static_cast<uint32_t>(c - 'A' + 10);
								} else {
									return std::nullopt;
								}
							}
							pos_ += 4;
							return value;
						}

						static void appendUtf8(std::string& out, uint32_t codepoint)
						{
							if (codepoint < 0x80u) {
								out.push_back(static_cast<char>(codepoint));
							} else if (codepoint < 0x800u) {
								out.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
								out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
							} else if (codepoint < 0x10000u) {
								out.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
								out.push_back(
									static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
								out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
							} else {
								out.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
								out.push_back(
									static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
								out.push_back(
									static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
								out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
							}
						}

						[[nodiscard]] std::optional<Value> parseNumber()
						{
							const std::size_t start = pos_;
							if (consume('-')) {
							}
							while (pos_ < text_.size() &&
								   ((text_[pos_] >= '0' && text_[pos_] <= '9') ||
									text_[pos_] == '.' || text_[pos_] == 'e' ||
									text_[pos_] == 'E' || text_[pos_] == '+' ||
									text_[pos_] == '-')) {
								++pos_;
							}
							if (pos_ == start) {
								return fail("expected a value");
							}
							const std::string token(text_.substr(start, pos_ - start));
							try {
								std::size_t consumed = 0;
								const double value = std::stod(token, &consumed);
								if (consumed != token.size()) {
									pos_ = start;
									return fail("malformed number");
								}
								return Value(Value::Storage(value));
							} catch (...) {
								pos_ = start;
								return fail("malformed number");
							}
						}
					};
				} /* namespace detail */

				/**************************************************************************/ /**
				 \brief      Parse a JSON document.
				 \param[in]  text   The complete document text.
				 \param[out] error  Filled with a description on failure (untouched
									on success).
				 \return     The parsed root value, or std::nullopt on failure.
				 *******************************************************************************/
				[[nodiscard]] inline std::optional<Value> parse(std::string_view text,
																std::string& error)
				{
					detail::Parser parser(text, error);
					return parser.run();
				}

			} /* namespace MiniJson */
		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_TESTS_SUPPORT_MINI_JSON_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
