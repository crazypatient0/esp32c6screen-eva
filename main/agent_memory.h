#ifndef AGENT_MEMORY_H
#define AGENT_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

// Maximum size of agent memory (≈1KB, fits comfortably in NVS)
#define AGENT_MEMORY_MAX_SIZE  1024

// NVS key for persistent storage
#define AGENT_MEMORY_NVS_KEY   "agent_memory"

// Initialize: load memory from NVS into RAM buffer.
// Called once at agent startup.
void agent_memory_init(void);

// Get current memory text for injection into system prompt.
// Returns empty string if no memory stored.
// Output is guaranteed null-terminated within buf_size bytes.
void agent_memory_get(char *buf, size_t buf_size);

// Update memory with a new exchange (user message + assistant response).
// Both args are mandatory. Internally formats as "User: ...\nAssistant: ..."\n
// Appends to rolling buffer; truncates oldest content if total > AGENT_MEMORY_MAX_SIZE.
// Persists to NVS immediately.
void agent_memory_append(const char *user_msg, const char *assistant_response);

// Clear all stored memory (both RAM and NVS).
void agent_memory_reset(void);

#endif // AGENT_MEMORY_H
