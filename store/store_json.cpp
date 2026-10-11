/* Bounded JSON reader for the PXA device catalog envelope.
 *
 * The Guest SDK has no libc, so this reader never allocates and never uses
 * memcpy/memset. Unknown fields are skipped, truncated strings keep the
 * already decoded prefix and malformed input fails closed. */

#include "store_json.h"

#define STORE_JSON_MAX_DEPTH 12
#define STORE_JSON_KEY_BYTES 32

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} store_json_t;

typedef struct {
    store_json_t json;
    uint8_t first;
} store_json_iterator_t;

static int text_equal(const char *left, const char *right) {
    size_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static size_t text_length(const char *value) {
    size_t length = 0;
    while (value[length] != '\0') ++length;
    return length;
}

static void string_reset(char *value) { value[0] = '\0'; }

static int json_whitespace(uint8_t value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static void json_skip_whitespace(store_json_t *json) {
    while (json->offset < json->size &&
           json_whitespace(json->data[json->offset])) ++json->offset;
}

static int json_peek(store_json_t *json, uint8_t *value) {
    json_skip_whitespace(json);
    if (json->offset >= json->size) return 0;
    *value = json->data[json->offset];
    return 1;
}

/* Appends one byte unless the output is already full; truncation is not an
 * error so a long summary still renders its decoded prefix. */
static void append_byte(char *output, size_t capacity, size_t *length,
                        uint8_t value) {
    if (*length + 1u >= capacity) return;
    output[*length] = (char)value;
    ++*length;
    output[*length] = '\0';
}

static void append_codepoint(char *output, size_t capacity, size_t *length,
                             uint32_t codepoint) {
    if (codepoint <= UINT32_C(0x7f)) {
        append_byte(output, capacity, length, (uint8_t)codepoint);
    } else if (codepoint <= UINT32_C(0x7ff)) {
        append_byte(output, capacity, length,
                    (uint8_t)(0xc0u | (codepoint >> 6)));
        append_byte(output, capacity, length,
                    (uint8_t)(0x80u | (codepoint & 0x3fu)));
    } else if (codepoint <= UINT32_C(0xffff) &&
               (codepoint < UINT32_C(0xd800) || codepoint > UINT32_C(0xdfff))) {
        append_byte(output, capacity, length,
                    (uint8_t)(0xe0u | (codepoint >> 12)));
        append_byte(output, capacity, length,
                    (uint8_t)(0x80u | ((codepoint >> 6) & 0x3fu)));
        append_byte(output, capacity, length,
                    (uint8_t)(0x80u | (codepoint & 0x3fu)));
    } else if (codepoint <= UINT32_C(0x10ffff)) {
        append_byte(output, capacity, length,
                    (uint8_t)(0xf0u | (codepoint >> 18)));
        append_byte(output, capacity, length,
                    (uint8_t)(0x80u | ((codepoint >> 12) & 0x3fu)));
        append_byte(output, capacity, length,
                    (uint8_t)(0x80u | ((codepoint >> 6) & 0x3fu)));
        append_byte(output, capacity, length,
                    (uint8_t)(0x80u | (codepoint & 0x3fu)));
    }
}

static int json_read_hex4(store_json_t *json, uint32_t *output) {
    uint32_t value = 0;
    size_t index;
    if (json->size - json->offset < 4u) return 0;
    for (index = 0; index < 4u; ++index) {
        uint8_t digit = json->data[json->offset + index];
        uint32_t nibble;
        if (digit >= '0' && digit <= '9') nibble = digit - '0';
        else if (digit >= 'a' && digit <= 'f') nibble = digit - 'a' + 10u;
        else if (digit >= 'A' && digit <= 'F') nibble = digit - 'A' + 10u;
        else return 0;
        value = (value << 4) | nibble;
    }
    json->offset += 4u;
    *output = value;
    return 1;
}

static int json_read_string(store_json_t *json, char *output, size_t capacity) {
    size_t length = 0;
    uint8_t current;
    if (output == NULL || capacity == 0) return 0;
    string_reset(output);
    if (!json_peek(json, &current) || current != '"') return 0;
    ++json->offset;
    while (json->offset < json->size) {
        uint8_t value = json->data[json->offset++];
        if (value == '"') return 1;
        if (value < 0x20u) return 0;
        if (value != '\\') {
            append_byte(output, capacity, &length, value);
            continue;
        }
        if (json->offset >= json->size) return 0;
        value = json->data[json->offset++];
        if (value == 'u') {
            uint32_t codepoint;
            if (!json_read_hex4(json, &codepoint)) return 0;
            if (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdbff)) {
                uint32_t low;
                if (json->size - json->offset < 6u ||
                    json->data[json->offset] != '\\' ||
                    json->data[json->offset + 1u] != 'u') return 0;
                json->offset += 2u;
                if (!json_read_hex4(json, &low) || low < UINT32_C(0xdc00) ||
                    low > UINT32_C(0xdfff)) return 0;
                codepoint = UINT32_C(0x10000) +
                            ((codepoint - UINT32_C(0xd800)) << 10) +
                            (low - UINT32_C(0xdc00));
            }
            append_codepoint(output, capacity, &length, codepoint);
            continue;
        }
        if (value == 'b') value = '\b';
        else if (value == 'f') value = '\f';
        else if (value == 'n') value = '\n';
        else if (value == 'r') value = '\r';
        else if (value == 't') value = '\t';
        else if (value != '"' && value != '\\' && value != '/') return 0;
        append_byte(output, capacity, &length, value);
    }
    return 0;
}

static int json_skip_string(store_json_t *json) {
    uint8_t current;
    if (!json_peek(json, &current) || current != '"') return 0;
    ++json->offset;
    while (json->offset < json->size) {
        uint8_t value = json->data[json->offset++];
        if (value == '"') return 1;
        if (value < 0x20u) return 0;
        if (value == '\\') {
            if (json->offset >= json->size) return 0;
            ++json->offset;
        }
    }
    return 0;
}

static int json_enter(store_json_t *json, store_json_iterator_t *iterator,
                      uint8_t open) {
    uint8_t value;
    if (!json_peek(json, &value) || value != open) return 0;
    ++json->offset;
    iterator->json = *json;
    iterator->first = 1;
    return 1;
}

static int json_object_begin(store_json_t *json, store_json_iterator_t *it) {
    return json_enter(json, it, '{');
}

static int json_array_begin(store_json_t *json, store_json_iterator_t *it) {
    return json_enter(json, it, '[');
}

/* Advances one object or array entry. For objects the caller receives the key
 * and the value position; for arrays key is NULL. */
static int json_next(store_json_iterator_t *it, uint8_t close, char *key,
                     size_t capacity, int *more) {
    store_json_t *json = &it->json;
    uint8_t value;
    json_skip_whitespace(json);
    if (!it->first) {
        if (json->offset >= json->size) return 0;
        value = json->data[json->offset];
        if (value == close) {
            ++json->offset;
            *more = 0;
            return 1;
        }
        if (value != ',') return 0;
        ++json->offset;
    }
    it->first = 0;
    json_skip_whitespace(json);
    if (json->offset >= json->size) return 0;
    if (json->data[json->offset] == close) {
        ++json->offset;
        *more = 0;
        return 1;
    }
    if (key != NULL) {
        if (!json_read_string(json, key, capacity)) return 0;
        json_skip_whitespace(json);
        if (json->offset >= json->size || json->data[json->offset] != ':')
            return 0;
        ++json->offset;
        json_skip_whitespace(json);
    }
    *more = 1;
    return 1;
}

static int json_skip_value(store_json_t *json, uint8_t depth) {
    uint8_t value;
    size_t start;
    if (depth > STORE_JSON_MAX_DEPTH) return 0;
    if (!json_peek(json, &value)) return 0;
    if (value == '"') return json_skip_string(json);
    if (value == '{' || value == '[') {
        store_json_iterator_t it;
        char key[STORE_JSON_KEY_BYTES];
        int more;
        if (value == '{') {
            if (!json_object_begin(json, &it)) return 0;
        } else if (!json_array_begin(json, &it)) {
            return 0;
        }
        for (;;) {
            if (!json_next(&it, value == '{' ? '}' : ']',
                           value == '{' ? key : NULL, sizeof(key), &more))
                return 0;
            if (!more) break;
            if (!json_skip_value(&it.json, (uint8_t)(depth + 1u))) return 0;
        }
        *json = it.json;
        return 1;
    }
    start = json->offset;
    while (json->offset < json->size) {
        value = json->data[json->offset];
        if (value == ',' || value == '}' || value == ']' ||
            json_whitespace(value)) break;
        ++json->offset;
    }
    return json->offset > start;
}

static int json_read_u64(store_json_t *json, uint64_t *output) {
    uint64_t value = 0;
    size_t digits = 0;
    json_skip_whitespace(json);
    while (json->offset < json->size) {
        uint8_t digit = json->data[json->offset];
        if (digit < '0' || digit > '9') break;
        if (value > (UINT64_MAX - (uint64_t)(digit - '0')) / 10u) return 0;
        value = value * 10u + (uint64_t)(digit - '0');
        ++json->offset;
        ++digits;
    }
    if (digits == 0) return 0;
    *output = value;
    return 1;
}

static int json_read_u16(store_json_t *json, uint16_t *output) {
    uint64_t value;
    if (!json_read_u64(json, &value) || value > UINT16_MAX) return 0;
    *output = (uint16_t)value;
    return 1;
}

static int json_read_bool(store_json_t *json, uint8_t *output) {
    static const char true_literal[] = "true";
    static const char false_literal[] = "false";
    const char *literal = true_literal;
    uint8_t value = 1;
    size_t index = 0;
    uint8_t current;
    if (!json_peek(json, &current)) return 0;
    if (current == 'f') {
        literal = false_literal;
        value = 0;
    }
    while (literal[index] != '\0') {
        if (json->offset + index >= json->size ||
            json->data[json->offset + index] != (uint8_t)literal[index])
            return 0;
        ++index;
    }
    json->offset += index;
    *output = value;
    return 1;
}

/* Appends a "/"-separated list element. */
static void append_list(char *output, size_t capacity, size_t *length,
                        const char *value, size_t value_length) {
    if (*length != 0) append_byte(output, capacity, length, '/');
    for (size_t index = 0; index < value_length; ++index)
        append_byte(output, capacity, length, (uint8_t)value[index]);
}

static void app_reset(store_app_t *app) {
    string_reset(app->app_id);
    string_reset(app->name);
    string_reset(app->summary);
    string_reset(app->version);
    string_reset(app->changelog);
    string_reset(app->tags);
    string_reset(app->publisher_key_id);
    string_reset(app->sha256);
    string_reset(app->download_ticket_url);
    string_reset(app->target_profile);
    string_reset(app->platforms);
    string_reset(app->architectures);
    string_reset(app->min_sdk);
    string_reset(app->target_sdk);
    string_reset(app->channel);
    app->size = 0;
    app->release_sequence = 0;
    app->permission_count = 0;
    app->service_count = 0;
}

static int parse_permission(store_json_t *json, store_permission_t *permission) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    string_reset(permission->name);
    string_reset(permission->scope);
    permission->required = 0;
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "name")) {
            if (!json_read_string(&it.json, permission->name,
                                  sizeof(permission->name))) return 0;
        } else if (text_equal(key, "scope")) {
            if (!json_read_string(&it.json, permission->scope,
                                  sizeof(permission->scope))) return 0;
        } else if (text_equal(key, "required")) {
            if (!json_read_bool(&it.json, &permission->required)) return 0;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_service(store_json_t *json, store_service_t *service) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    string_reset(service->name);
    service->service_id = 0;
    service->min_major = 0;
    service->min_minor = 0;
    service->max_major = 0;
    service->max_minor = 0;
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "name")) {
            if (!json_read_string(&it.json, service->name,
                                  sizeof(service->name))) return 0;
        } else if (text_equal(key, "service_id")) {
            if (!json_read_u16(&it.json, &service->service_id)) return 0;
        } else if (text_equal(key, "min_major")) {
            if (!json_read_u16(&it.json, &service->min_major)) return 0;
        } else if (text_equal(key, "min_minor")) {
            if (!json_read_u16(&it.json, &service->min_minor)) return 0;
        } else if (text_equal(key, "max_major")) {
            if (!json_read_u16(&it.json, &service->max_major)) return 0;
        } else if (text_equal(key, "max_minor")) {
            if (!json_read_u16(&it.json, &service->max_minor)) return 0;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_permissions(store_json_t *json, store_app_t *app) {
    store_json_iterator_t it;
    int more;
    if (!json_array_begin(json, &it)) return 0;
    for (;;) {
        if (!json_next(&it, ']', NULL, 0, &more)) return 0;
        if (!more) break;
        if (app->permission_count >= STORE_MAX_PERMISSIONS) {
            if (!json_skip_value(&it.json, 1)) return 0;
            continue;
        }
        if (!parse_permission(&it.json,
                              &app->permissions[app->permission_count]))
            return 0;
        ++app->permission_count;
    }
    *json = it.json;
    return 1;
}

static int parse_services(store_json_t *json, store_app_t *app) {
    store_json_iterator_t it;
    int more;
    if (!json_array_begin(json, &it)) return 0;
    for (;;) {
        if (!json_next(&it, ']', NULL, 0, &more)) return 0;
        if (!more) break;
        if (app->service_count >= STORE_MAX_SERVICES) {
            if (!json_skip_value(&it.json, 1)) return 0;
            continue;
        }
        if (!parse_service(&it.json, &app->services[app->service_count]))
            return 0;
        ++app->service_count;
    }
    *json = it.json;
    return 1;
}

static int parse_string_list(store_json_t *json, char *output,
                             size_t capacity) {
    store_json_iterator_t it;
    char item[STORE_MAX_PROFILE];
    int more;
    size_t length = 0;
    string_reset(output);
    if (!json_array_begin(json, &it)) return 0;
    for (;;) {
        if (!json_next(&it, ']', NULL, 0, &more)) return 0;
        if (!more) break;
        if (!json_read_string(&it.json, item, sizeof(item))) return 0;
        append_list(output, capacity, &length, item, text_length(item));
    }
    *json = it.json;
    return 1;
}

static int parse_package_targets(store_json_t *json, store_app_t *app) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "platforms")) {
            if (!parse_string_list(&it.json, app->platforms,
                                   sizeof(app->platforms))) return 0;
        } else if (text_equal(key, "architectures")) {
            if (!parse_string_list(&it.json, app->architectures,
                                   sizeof(app->architectures))) return 0;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_compatibility(store_json_t *json, store_app_t *app) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "permissions")) {
            if (!parse_permissions(&it.json, app)) return 0;
        } else if (text_equal(key, "service_requirements")) {
            if (!parse_services(&it.json, app)) return 0;
        } else if (text_equal(key, "package_targets")) {
            if (!parse_package_targets(&it.json, app)) return 0;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_app(store_json_t *json, store_app_t *app) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    app_reset(app);
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "app_id")) {
            if (!json_read_string(&it.json, app->app_id,
                                  sizeof(app->app_id))) return 0;
        } else if (text_equal(key, "name")) {
            if (!json_read_string(&it.json, app->name,
                                  sizeof(app->name))) return 0;
        } else if (text_equal(key, "summary")) {
            if (!json_read_string(&it.json, app->summary,
                                  sizeof(app->summary))) return 0;
        } else if (text_equal(key, "version")) {
            if (!json_read_string(&it.json, app->version,
                                  sizeof(app->version))) return 0;
        } else if (text_equal(key, "changelog")) {
            if (!json_read_string(&it.json, app->changelog,
                                  sizeof(app->changelog))) return 0;
        } else if (text_equal(key, "tags")) {
            if (!json_read_string(&it.json, app->tags,
                                  sizeof(app->tags))) return 0;
        } else if (text_equal(key, "publisher_key_id")) {
            if (!json_read_string(&it.json, app->publisher_key_id,
                                  sizeof(app->publisher_key_id))) return 0;
        } else if (text_equal(key, "sha256")) {
            if (!json_read_string(&it.json, app->sha256,
                                  sizeof(app->sha256))) return 0;
        } else if (text_equal(key, "download_ticket_url")) {
            if (!json_read_string(&it.json, app->download_ticket_url,
                                  sizeof(app->download_ticket_url))) return 0;
        } else if (text_equal(key, "target_profile")) {
            if (!json_read_string(&it.json, app->target_profile,
                                  sizeof(app->target_profile))) return 0;
        } else if (text_equal(key, "min_sdk")) {
            if (!json_read_string(&it.json, app->min_sdk,
                                  sizeof(app->min_sdk))) return 0;
        } else if (text_equal(key, "target_sdk")) {
            if (!json_read_string(&it.json, app->target_sdk,
                                  sizeof(app->target_sdk))) return 0;
        } else if (text_equal(key, "channel")) {
            if (!json_read_string(&it.json, app->channel,
                                  sizeof(app->channel))) return 0;
        } else if (text_equal(key, "size")) {
            if (!json_read_u64(&it.json, &app->size)) return 0;
        } else if (text_equal(key, "release_sequence")) {
            if (!json_read_u64(&it.json, &app->release_sequence)) return 0;
        } else if (text_equal(key, "compatibility")) {
            if (!parse_compatibility(&it.json, app)) return 0;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_items(store_json_t *json, store_catalog_t *catalog) {
    store_json_iterator_t it;
    int more;
    if (!json_array_begin(json, &it)) return 0;
    for (;;) {
        if (!json_next(&it, ']', NULL, 0, &more)) return 0;
        if (!more) break;
        if (!parse_app(&it.json, store_catalog_slot(catalog))) return 0;
    }
    *json = it.json;
    return 1;
}

static int parse_taxonomy_entry(store_json_t *json, store_taxonomy_entry_t *entry,
                               int with_kind) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    string_reset(entry->kind);
    string_reset(entry->value);
    string_reset(entry->label);
    string_reset(entry->label_en);
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (with_kind && text_equal(key, "kind")) {
            if (!json_read_string(&it.json, entry->kind,
                                  sizeof(entry->kind))) return 0;
        } else if (text_equal(key, "value")) {
            if (!json_read_string(&it.json, entry->value,
                                  sizeof(entry->value))) return 0;
        } else if (text_equal(key, "label")) {
            if (!json_read_string(&it.json, entry->label,
                                  sizeof(entry->label))) return 0;
        } else if (text_equal(key, "label_en")) {
            if (!json_read_string(&it.json, entry->label_en,
                                  sizeof(entry->label_en))) return 0;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_taxonomy(store_json_t *json, store_taxonomy_t *taxonomy) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    taxonomy->kind_count = 0;
    taxonomy->category_count = 0;
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "kinds")) {
            store_json_iterator_t list;
            int list_more;
            if (!json_array_begin(&it.json, &list)) return 0;
            for (;;) {
                if (!json_next(&list, ']', NULL, 0, &list_more)) return 0;
                if (!list_more) break;
                if (taxonomy->kind_count >= STORE_MAX_KINDS ||
                    !parse_taxonomy_entry(
                        &list.json,
                        &taxonomy->kinds[taxonomy->kind_count], 0))
                    return 0;
                ++taxonomy->kind_count;
            }
            it.json = list.json;
        } else if (text_equal(key, "categories")) {
            store_json_iterator_t list;
            int list_more;
            if (!json_array_begin(&it.json, &list)) return 0;
            for (;;) {
                if (!json_next(&list, ']', NULL, 0, &list_more)) return 0;
                if (!list_more) break;
                if (taxonomy->category_count >= STORE_MAX_TAXONOMY) {
                    if (!json_skip_value(&list.json, 1)) return 0;
                    continue;
                }
                if (!parse_taxonomy_entry(
                        &list.json,
                        &taxonomy->categories[taxonomy->category_count], 1))
                    return 0;
                ++taxonomy->category_count;
            }
            it.json = list.json;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return 1;
}

static int parse_payload(store_json_t *json, store_catalog_t *catalog,
                         store_taxonomy_t *taxonomy) {
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    int seen_items = 0;
    catalog->has_more = 0;
    catalog->next_cursor = 0;
    catalog->dropped = 0;
    if (!json_object_begin(json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "items")) {
            if (!parse_items(&it.json, catalog)) return 0;
            seen_items = 1;
        } else if (taxonomy != NULL && text_equal(key, "taxonomy")) {
            if (!parse_taxonomy(&it.json, taxonomy)) return 0;
        } else if (text_equal(key, "next_cursor")) {
            uint8_t value;
            if (!json_peek(&it.json, &value)) return 0;
            if (value == 'n') {
                if (!json_skip_value(&it.json, 1)) return 0;
            } else {
                if (!json_read_u64(&it.json, &catalog->next_cursor)) return 0;
                catalog->has_more = 1;
            }
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    *json = it.json;
    return seen_items;
}

int store_json_parse_catalog(const uint8_t *data, size_t size,
                             store_catalog_t *catalog,
                             store_taxonomy_t *taxonomy) {
    store_json_t json;
    store_json_iterator_t it;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    int parsed = 0;
    if (data == NULL || catalog == NULL) return 0;
    json.data = data;
    json.size = size;
    json.offset = 0;
    if (!json_object_begin(&json, &it)) return 0;
    while (json_next(&it, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "payload")) {
            if (!parse_payload(&it.json, catalog, taxonomy)) return 0;
            parsed = 1;
        } else if (!json_skip_value(&it.json, 1)) {
            return 0;
        }
    }
    return parsed;
}

int store_json_parse_app_detail(const uint8_t *data, size_t size,
                                store_app_t *app) {
    store_json_t json;
    store_json_iterator_t envelope;
    store_json_iterator_t payload;
    char key[STORE_JSON_KEY_BYTES];
    int more;
    int parsed = 0;
    if (data == NULL || app == NULL) return 0;
    json.data = data;
    json.size = size;
    json.offset = 0;
    if (!json_object_begin(&json, &envelope)) return 0;
    while (json_next(&envelope, '}', key, sizeof(key), &more)) {
        if (!more) break;
        if (text_equal(key, "payload")) {
            store_json_t payload_json = envelope.json;
            if (!json_object_begin(&payload_json, &payload)) return 0;
            while (json_next(&payload, '}', key, sizeof(key), &more)) {
                if (!more) break;
                if (text_equal(key, "item")) {
                    if (!parse_app(&payload.json, app)) return 0;
                    parsed = 1;
                } else if (!json_skip_value(&payload.json, 1)) {
                    return 0;
                }
            }
            envelope.json = payload.json;
        } else if (!json_skip_value(&envelope.json, 1)) {
            return 0;
        }
    }
    return parsed;
}
