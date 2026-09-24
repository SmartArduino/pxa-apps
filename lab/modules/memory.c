#define PXA_LAB_MODULE_PREFIX pxa_lab_memory_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"

#define MEMORY_HEAP_BYTES 512u
#define MEMORY_ALIGN 8u
#define MEMORY_INITIAL_WORDS 4u
#define MEMORY_GROWN_WORDS 8u
#define MEMORY_CHECKS 5u

typedef struct {
    uint32_t length;
    uint8_t used;
} memory_block_t;

typedef union {
    uint64_t alignment;
    uint8_t bytes[MEMORY_HEAP_BYTES];
} memory_heap_t;

static uint8_t packet[1664];
static memory_heap_t heap;
static uint8_t passed_checks;
static uint8_t has_run;

static uint32_t align_size(uint32_t size) {
    return (size + MEMORY_ALIGN - 1u) & ~(MEMORY_ALIGN - 1u);
}

static void heap_reset(void) {
    memory_block_t *first = (memory_block_t *)(void *)heap.bytes;
    first->length = MEMORY_HEAP_BYTES - sizeof(*first);
    first->used = 0;
}

static memory_block_t *next_block(memory_block_t *block) {
    uint8_t *next = (uint8_t *)(void *)(block + 1) + block->length;
    const uint32_t offset = (uint32_t)(next - heap.bytes);
    return offset <= MEMORY_HEAP_BYTES - sizeof(memory_block_t)
               ? (memory_block_t *)(void *)next : NULL;
}

static void merge_free_blocks(void) {
    memory_block_t *block = (memory_block_t *)(void *)heap.bytes;
    memory_block_t *next;
    while ((next = next_block(block)) != NULL) {
        if (!block->used && !next->used) {
            block->length += (uint32_t)sizeof(*next) + next->length;
            continue;
        }
        block = next;
    }
}

static void *heap_alloc(uint32_t size) {
    memory_block_t *block = (memory_block_t *)(void *)heap.bytes;
    const uint32_t requested = align_size(size);
    memory_block_t *next;

    if (size == 0 || requested < size) return NULL;
    do {
        if (!block->used && block->length >= requested) {
            const uint32_t remainder = block->length - requested;
            if (remainder >= sizeof(*block) + MEMORY_ALIGN) {
                next = (memory_block_t *)(void *)((uint8_t *)(void *)(block + 1) +
                                                   requested);
                next->length = remainder - sizeof(*next);
                next->used = 0;
                block->length = requested;
            }
            block->used = 1;
            return block + 1;
        }
        block = next_block(block);
    } while (block != NULL);
    return NULL;
}

static void *heap_calloc(uint32_t count, uint32_t size) {
    uint8_t *memory;
    uint32_t index;
    if (count == 0 || size == 0 || count > UINT32_MAX / size) return NULL;
    memory = (uint8_t *)heap_alloc(count * size);
    if (memory == NULL) return NULL;
    for (index = 0; index < count * size; ++index) memory[index] = 0;
    return memory;
}

static void heap_free(void *memory) {
    memory_block_t *block;
    if (memory == NULL) return;
    block = (memory_block_t *)memory - 1;
    if ((uint8_t *)(void *)block < heap.bytes ||
        (uint8_t *)(void *)block >= heap.bytes + MEMORY_HEAP_BYTES || !block->used) {
        return;
    }
    block->used = 0;
    merge_free_blocks();
}

static void *heap_realloc(void *memory, uint32_t size) {
    memory_block_t *block;
    uint8_t *replacement;
    uint32_t copy_length;
    uint32_t index;
    if (memory == NULL) return heap_alloc(size);
    if (size == 0) {
        heap_free(memory);
        return NULL;
    }
    block = (memory_block_t *)memory - 1;
    if (!block->used) return NULL;
    if (size <= block->length) return memory;
    replacement = (uint8_t *)heap_alloc(size);
    if (replacement == NULL) return NULL;
    copy_length = block->length;
    for (index = 0; index < copy_length; ++index)
        replacement[index] = ((uint8_t *)memory)[index];
    heap_free(memory);
    return replacement;
}

static uint8_t run_memory_checks(void) {
    static const uint32_t source[MEMORY_INITIAL_WORDS] = {3, 5, 7, 11};
    uint32_t *words;
    uint32_t *grown;
    void *separator;
    void *reuse;
    uint8_t passed = 0;
    uint32_t index;

    heap_reset();
    words = (uint32_t *)heap_calloc(MEMORY_INITIAL_WORDS, sizeof(*words));
    if (words == NULL) return 0;
    passed += words[0] == 0 && words[MEMORY_INITIAL_WORDS - 1u] == 0;
    for (index = 0; index < MEMORY_INITIAL_WORDS; ++index) words[index] = source[index];
    passed += words[0] == source[0] && words[MEMORY_INITIAL_WORDS - 1u] == source[3];

    separator = heap_alloc(32);
    grown = (uint32_t *)heap_realloc(words, MEMORY_GROWN_WORDS * sizeof(*words));
    passed += grown != NULL;
    if (grown != NULL) {
        for (index = 0; index < MEMORY_INITIAL_WORDS; ++index)
            if (grown[index] != source[index]) break;
        passed += index == MEMORY_INITIAL_WORDS;
    }
    heap_free(separator);
    heap_free(grown);
    reuse = heap_alloc(128);
    passed += reuse != NULL;
    heap_free(reuse);
    return passed;
}

static int render(void) {
    static const char body[] = "小型堆的 calloc、realloc、free、合并和复用";
    static const char ready[] = "点击运行一次动态内存生命周期检查";
    static const char success[] = "5/5 项通过: 分配、扩容、释放和复用";
    static const char failure[] = "堆分配或数据校验失败，可再次运行";
    const char *status = !has_run ? ready
                       : passed_checks == MEMORY_CHECKS ? success : failure;
    pxa_ui_demo_page_t page = {
        "动态内存", body, status, "运行分配检查", NULL,
        PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_PROGRESS, 8,
        (uint8_t)(passed_checks * 100u / MEMORY_CHECKS), 0, 0, 1};

    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    has_run = 0;
    passed_checks = 0;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK
                                                : PXA_STATUS_RESOURCE_LIMIT;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (!pxa_ui_parse_event(&parsed, &ui_event) ||
        ui_event.node != PXA_UI_DEMO_PAGE_NODE_BUTTON ||
        ui_event.kind != PXA_UI_EVENT_CLICK_KIND) {
        return PXA_EVENT_UNHANDLED;
    }
    passed_checks = run_memory_checks();
    has_run = 1;
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
}
