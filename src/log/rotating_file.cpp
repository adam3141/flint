#include "flint/log/rotating_file.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <string_view>
#include <system_error>
#include <utility>

namespace flint::log {

auto RotatingFile::Open(std::filesystem::path path, RotationOptions rotation) -> std::expected<RotatingFile, std::error_code> {
  std::error_code error;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      return std::unexpected(error);
    }
  }
  std::ofstream stream{path, std::ios::binary | std::ios::app};
  if (!stream.is_open()) {
    return std::unexpected(std::make_error_code(std::errc::permission_denied));
  }
  std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error) {
    size = 0;
  }
  return RotatingFile{std::move(path), rotation, std::move(stream), size};
}

RotatingFile::RotatingFile(std::filesystem::path path, RotationOptions rotation, std::ofstream stream, std::uintmax_t size)
    : m_path{std::move(path)}, m_rotation{rotation}, m_stream{std::move(stream)}, m_size{size} {}

auto RotatingFile::Append(std::string_view text) -> void {
  if (m_rotation.maxBytes > 0 && m_size > 0 && m_size + text.size() > m_rotation.maxBytes) {
    Rotate();
  }
  m_stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  m_size += text.size();
}

auto RotatingFile::Flush() -> void {
  m_stream.flush();
}

auto RotatingFile::RotatedPath(std::size_t index) const -> std::filesystem::path {
  // app.log -> app.1.log; app.jsonl -> app.1.jsonl
  std::filesystem::path rotated = m_path;
  rotated.replace_filename(std::format("{}.{}{}", m_path.stem().string(), index, m_path.extension().string()));
  return rotated;
}

auto RotatingFile::Rotate() -> void {
  m_stream.close();
  std::error_code error;
  if (m_rotation.maxFiles == 0) {
    std::filesystem::remove(m_path, error);
  } else {
    std::filesystem::remove(RotatedPath(m_rotation.maxFiles), error);
    for (std::size_t index = m_rotation.maxFiles; index > 1; --index) {
      std::filesystem::rename(RotatedPath(index - 1), RotatedPath(index), error);
    }
    std::filesystem::rename(m_path, RotatedPath(1), error);
  }
  m_stream.open(m_path, std::ios::binary | std::ios::trunc);
  m_size = 0;
}

}  // namespace flint::log
