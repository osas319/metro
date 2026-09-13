#!/usr/bin/env bash
# İstasyon asset doğrulayıcısı — docs/MODELING.md §15 kurallarını denetler.
#
# Kullanım:  tools/validate_station_assets.sh [istasyon-dizini]
# Varsayılan: assets/stations/kadikoy
#
# Çıkış kodu: 0 = hata yok (uyarı olabilir), 1 = en az bir hata var.
# Bu script hiçbir şeyi değiştirmez; sadece okur ve raporlar.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATION_DIR="${1:-${ROOT}/assets/stations/kadikoy}"
MANIFEST="${STATION_DIR}/station.json"

errors=0
warnings=0

err()  { printf '  HATA   %s\n' "$*"; errors=$((errors + 1)); }
warn() { printf '  UYARI  %s\n' "$*"; warnings=$((warnings + 1)); }
note() { printf '         %s\n' "$*"; }

rel() { # mutlak yolu proje köküne göre kısalt (rapor okunabilirliği)
  local p="$1"
  case "$p" in
    "${ROOT}/"*) printf '%s' "${p#"${ROOT}/"}" ;;
    *) printf '%s' "$p" ;;
  esac
}

usage() {
  cat <<'EOF'
Kullanım: tools/validate_station_assets.sh [istasyon-dizini]

Bir istasyonun station.json manifestini ve asset ağacını docs/MODELING.md
kurallarına göre denetler. Örnek:

  tools/validate_station_assets.sh
  tools/validate_station_assets.sh assets/stations/kadikoy
EOF
}

case "${1:-}" in
  -h | --help)
    usage
    exit 0
    ;;
esac

printf 'İstasyon asset doğrulaması: %s\n' "$(rel "${STATION_DIR}")"

if [[ ! -f "${MANIFEST}" ]]; then
  err "manifest bulunamadı: $(rel "${MANIFEST}")"
  printf '\nSonuç: %d hata, %d uyarı\n' "${errors}" "${warnings}"
  exit 1
fi

# --- manifest alanı okuma ---------------------------------------------------
# Manifest şeması düz ve tek seviyeli olduğu için tam bir JSON ayrıştırıcıya
# gerek yok: ilk "\"anahtar\": \"değer\"" eşleşmesi yeterli.
json_string() {
  grep -oE "\"$1\"[[:space:]]*:[[:space:]]*\"[^\"]*\"" "${MANIFEST}" 2>/dev/null |
    head -n1 |
    sed -E 's/.*:[[:space:]]*"([^"]*)"/\1/'
}

# Manifest alanındaki yolu, motorun kullandığı arama sırasıyla çözer:
# mutlak → proje köküne göreli → manifest dizinine göreli.
# Motor (StationManifest::load) tam olarak bu sırayı izliyor; burada aynısını
# taklit ediyoruz ki doğrulayıcı yanlış güvence vermesin.
resolve_path() {
  local p="$1"
  [[ -z "${p}" ]] && return 1
  if [[ "${p}" == /* ]]; then
    [[ -e "${p}" ]] && return 0
    return 1
  fi
  [[ -e "${ROOT}/${p}" ]] && return 0
  [[ -e "${STATION_DIR}/${p}" ]] && return 0
  return 1
}

# --- 1. zorunlu alanlar ----------------------------------------------------
for key in name line meshes materials textures lightmaps; do
  if [[ -z "$(json_string "${key}")" ]]; then
    err "manifest alanı eksik veya boş: \"${key}\" (yükleme başarısız olur)"
  fi
done

# --- 2. manifest'te bildirilen dizinler ------------------------------------
for key in meshes materials textures lightmaps; do
  dir="$(json_string "${key}")"
  [[ -z "${dir}" ]] && continue
  if [[ "${dir}" == /* ]]; then
    target="${dir}"
  elif [[ -d "${STATION_DIR}/${dir}" ]]; then
    target="${STATION_DIR}/${dir}"
  else
    target="${ROOT}/${dir}"
  fi
  if [[ ! -d "${target}" ]]; then
    err "dizin yok: \"${key}\" = \"${dir}\" (beklenen: $(rel "${STATION_DIR}/${dir}"))"
  fi
done

# --- 3. dosya yolu gerektiren alanlar --------------------------------------
model="$(json_string model)"
if [[ -z "${model}" ]]; then
  warn "\"model\" alanı yok — renderer placeholder sahneyi çoğaltmaya devam eder"
else
  resolve_path "${model}" ||
    err "model dosyası yok: \"${model}\" (renderer başlatılamaz, uygulama açılmaz)"
fi

navmesh="$(json_string navmesh)"
if [[ -n "${navmesh}" ]]; then
  resolve_path "${navmesh}" ||
    err "navmesh dosyası yok: \"${navmesh}\" (StationManifest::load başarısız olur, uygulama açılmaz)"
fi

audio="$(json_string audio_directory)"
if [[ -n "${audio}" ]]; then
  resolve_path "${audio}" ||
    warn "audio_directory yok: \"${audio}\" (yalnızca loglanır, çalışmayı engellemez)"
fi

# --- 4. mesh adlandırma ve LOD bütünlüğü -----------------------------------
mesh_dir="$(json_string meshes)"
mesh_dir="${mesh_dir:-meshes}"
[[ -d "${STATION_DIR}/${mesh_dir}" ]] || mesh_dir=""
if [[ -n "${mesh_dir}" ]]; then
  mesh_path="${STATION_DIR}/${mesh_dir}"
  # Şema: <kod>_<parça>[_<varyant>][_lodN][_col].glb   (kod: 3 harf veya sh)
  name_re='^(sh|[a-z]{3})_[a-z0-9_]+(_lod[0-2])?(_col)?\.(glb|gltf)$'
  lod_seen=0
  shopt -s nullglob
  for f in "${mesh_path}"/*.glb "${mesh_path}"/*.gltf; do
    base="$(basename "${f}")"
    stem="${base%.*}"
    if ! grep -qE "${name_re}" <<<"${base}"; then
      err "mesh adı şemaya uymuyor: ${mesh_dir}/${base}"
      note "beklenen: <kod>_<parca>[_<varyant>][_lod0..2][_col].glb  (docs/MODELING.md §5)"
      continue
    fi
    # LOD bütünlüğü
    if [[ "${stem}" == *_lod0 ]]; then
      lod_seen=1
      parca="${stem%_lod0}"
      for n in 1 2; do
        [[ -f "${mesh_path}/${parca}_lod${n}.glb" ]] ||
          warn "LOD${n} eksik: ${mesh_dir}/${parca}_lod${n}.glb"
      done
      [[ -f "${mesh_path}/${parca}_col.glb" ]] ||
        warn "collision mesh eksik: ${mesh_dir}/${parca}_col.glb"
    elif [[ "${stem}" == *_lod1 || "${stem}" == *_lod2 ]]; then
      parca="${stem%_lod?}"
      [[ -f "${mesh_path}/${parca}_lod0.glb" ]] ||
        warn "LOD0 yok ama LOD${stem: -1} var: ${mesh_dir}/${base}"
    fi
    # ASCII dışı karakter
    if LC_ALL=C grep -q '[^ -~]' <<<"${base}"; then
      warn "mesh adında ASCII dışı karakter var: ${base}"
    fi
  done
  shopt -u nullglob
  if [[ "${lod_seen}" -eq 0 ]]; then
    note "mesh klasöründe LOD0 mesh yok — mimari asset henüz üretilmemiş olabilir"
  fi
fi

# --- 5. doku adlandırma ----------------------------------------------------
tex_dir="$(json_string textures)"
tex_dir="${tex_dir:-textures}"
if [[ -d "${STATION_DIR}/${tex_dir}" ]]; then
  shopt -s nullglob
  for f in "${STATION_DIR}/${tex_dir}"/*; do
    [[ -f "${f}" ]] || continue
    base="$(basename "${f}")"
    [[ "${base}" == .gitkeep ]] && continue
    if ! grep -qE '_[0-9]+k\.(ktx2|png|jpg|jpeg|exr)$' <<<"${base}"; then
      warn "doku adında boyut eki yok (beklenen _1k/_2k/_4k): ${tex_dir}/${base}"
    fi
    if ! grep -qE '\.ktx2$' <<<"${base}" && ! grep -qE '\.(exr)$' <<<"${base}"; then
      warn "KTX2 dışı doku (hedef format KTX2/Basis): ${tex_dir}/${base}"
    fi
    if LC_ALL=C grep -q '[^ -~]' <<<"${base}"; then
      warn "doku adında ASCII dışı karakter var: ${base}"
    fi
  done
  shopt -u nullglob
fi

# --- 6. özet ---------------------------------------------------------------
printf '\nSonuç: %d hata, %d uyarı\n' "${errors}" "${warnings}"
if [[ "${errors}" -gt 0 ]]; then
  exit 1
fi
if [[ "${warnings}" -gt 0 ]]; then
  printf 'Uyarılar teslimi engellemez ama gözden geçirilmeli.\n'
fi
exit 0
