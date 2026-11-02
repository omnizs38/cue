#pragma once
#include <string_view>
inline bool update_temp_directory_name(std::wstring_view name) {
  constexpr std::wstring_view prefix=L"Cue-update-";
  if(!name.starts_with(prefix)||name.size()!=prefix.size()+32)return false;
  for(wchar_t c:name.substr(prefix.size()))if(!((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f')))return false;
  return true;
}
