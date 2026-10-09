/**
  * @file handle.h
  * @brief Type-safe handle for resource references in LaMancha Engine
  * @author Alonso Moreno <ph0nsy>
  * @date 2026
  * @copyright Copyright (C) 2026 Alonso Moreno
  *
  * @details
  * A safe, opaque reference to an engine resource (texture, shader, sound, etc.).
  *
  * Each handle packs two 16-bit fields into one u32:
  * - Bits [31..16] = Generation counter (incremented each time a slot is reused)
  * - Bits [15..00] = Index (index into the resource pool, up to 65,535 entries)
  *
  * Wraps a u32 to prevent accidental mixing of handle types. Use different
  * types to create distinct handle types (like TextureHandle)
  *
  * The generation counter solves the "dangling handle" problem. When slot #42 is freed and
  * then reallocated for a new resource, its generation is incremented. Any old handle that
  * referenced slot #42 with the previous generation value is now stale. The system can detect
  * this by comparing the handle's generation to the pool's current generation for that slot.
  *
  * @tparam T Tag type
  * @note Zero (invalid handle) is reserved for "null" references
  * @note Handles are stable across frames but may be recycled after deletion
  * @note Handle<EntityTag> and Handle<TextureTag> are completely different C++ types, even though both store a u32.
  * @note Tag types (EntityTag, TextureTag, etc.) contain no data; they exist purely to create distinct template instantiations.
  *
  * Example:
  * @code
  * using ShaderTag = Handle;
  * using TextureHandle = Handle;
  * Shader shader{}
  * TextureHandle tex{shader};  // Compiles, but logically wrong - use different types!
  * @endcode
  */

#pragma once

template <typename Tag>
struct Handle
{ 
  Handle() noexcept = default;

  u32 value;  ///< Unique value, 0 = invalid (16-bit ID + 16-bit generation)
  static constexpr u32 INDEX_MASK = (1u << 16) - 1;

  /** @brief Get this Handle's identifier */
  u16 index() const { return value & INDEX_MASK; }

  /** @brief Get this Handle's generation */
  u16 generation() const { return value >> 16; }

  /** @brief Check if handle is valid (non-zero) */
  constexpr bool isNull() const { return value == 0; }

  /** @brief Equality comparison (both index and generation must match) */
  constexpr bool operator==(const Handle& other) const { return value == other.value; }
  constexpr bool operator!=(const Handle& other) const { return value != other.value; }

  /** @brief Create new Handle's of Tag type */
  static Handle create(u16 id, u16 gen)
  {
    return Handle{ (static_cast<u32>(gen) << 16) | static_cast<u32>(id) };
  }
};

/** @defgroup tags Tag types
 *  @brief Empty structs used as phantom types to make each handle type distinct
 *
 *  They have no size beyond what the ABI requires (usually 1 byte, but the
 *  compiler may not allocate any storage since they are never instantiated
 *  as standalone objects, only as template arguments)
 *
 *  @{
 */

struct TextureTag {};
struct ShaderTag {};
struct AudioTag {};
struct LevelTag {};
struct ScriptTag {};
struct FontTag {};

/** @} end of tags group */

/** @defgroup handles Handle aliases
 *  @brief The type system now prevents accidentally mixing IDs from different resource pools
 *  @{
 */

using TextureHandle = Handle<TextureTag>;
using ShaderHandle = Handle<ShaderTag>;
using AudioHandle = Handle<AudioTag>;
using LevelHandle = Handle<LevelTag>;
using ScriptHandle = Handle<ScriptTag>;
using FontHandle = Handle<FontTag>;

/** @} end of handles group */

