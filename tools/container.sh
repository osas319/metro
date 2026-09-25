#!/usr/bin/env bash
# Metro geliştirme konteyner yardımcısı — rootless Podman, host'a dokunmaz.
#
# GPU:      --device /dev/dri (kullanıcının erişim izinleri geçerli kalır)
# Pencere:  Wayland socket'i salt-bağlama ile içeri alınır; SDL_VIDEODRIVER
#           wayland'a sabitlenir (XWayland fallback'i yok).
# Dosyalar: repo /work altına mount edilir; --userns=keep-id sayesinde
#           container içinde üretilen dosyalar host'ta genc kullanıcısına ait.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="localhost/metro-dev:44"

XDG="${XDG_RUNTIME_DIR:-/run/user/1000}"
WL="${WAYLAND_DISPLAY:-wayland-0}"

run_args=(
  --rm
  --userns=keep-id
  --security-opt label=disable
  --device /dev/dri
  -e "XDG_RUNTIME_DIR=${XDG}"
  -e "SDL_VIDEODRIVER=wayland"
  -v "${ROOT}:/work"
  -w /work
)

# Wayland socket'i (yalnızca varsa bağla)
if [[ -S "${XDG}/${WL}" ]]; then
  run_args+=(-v "${XDG}/${WL}:${XDG}/${WL}")
else
  echo "UYARI: ${XDG}/${WL} bulunamadı — Wayland oturumu içinde misiniz?" >&2
fi

# Test/CI: METRO_AUTO_EXIT set edilmişse konteynere aktar
if [[ -n "${METRO_AUTO_EXIT:-}" ]]; then
  run_args+=(-e "METRO_AUTO_EXIT=${METRO_AUTO_EXIT}")
fi

# Çalıştırılacak GLB/glTF yolunu varsayılan örnek modelden değiştirme
if [[ -n "${METRO_MODEL_PATH:-}" ]]; then
  run_args+=(-e "METRO_MODEL_PATH=${METRO_MODEL_PATH}")
fi

if [[ -n "${METRO_STATION_MANIFEST:-}" ]]; then
  run_args+=(-e "METRO_STATION_MANIFEST=${METRO_STATION_MANIFEST}")
fi

ensure_submodules() {
  if [[ ! -d "${ROOT}/.git" && ! -f "${ROOT}/.git" ]]; then
    echo "HATA: Git metadata bulunamadı (${ROOT}/.git)." >&2
    echo "Repo'yu git clone ile aldıktan sonra tekrar çalıştırın." >&2
    return 1
  fi

  echo "Metro third_party submodule'leri hazırlanıyor..."
  podman run --rm \
    --userns=keep-id \
    -v "${ROOT}:/work" \
    -w /work \
    "${IMAGE}" \
    git submodule sync --recursive
  podman run --rm \
    --userns=keep-id \
    -v "${ROOT}:/work" \
    -w /work \
    "${IMAGE}" \
    git submodule update --init --recursive
}
usage() {
  cat <<EOF
Kullanım: tools/container.sh <komut> [arg]
  image          Geliştirme imajını derle
  configure      CMake configure + Ninja üretimi (arg: build type, default Debug)
  build          Derle (arg'ler cmake --build'e geçer)
  test           Simülasyon testlerini çalıştır
  shell          İnteraktif bash (repo /work altında mount'lu)
  run            Uygulamayı çalıştır (arg'ler exe'ye geçer)
  vulkaninfo     GPU / sürücü doğrulaması
EOF
}

cmd="${1:-}"; [[ $# -gt 0 ]] && shift

case "${cmd}" in
  image)      podman build -t "${IMAGE}" "${ROOT}" ;;
  configure)  ensure_submodules
              podman run "${run_args[@]}" "${IMAGE}" \
                cmake -S /work -B /work/build -G Ninja \
                -DCMAKE_BUILD_TYPE="${1:-Debug}" ;;
  build)      ensure_submodules
              podman run "${run_args[@]}" "${IMAGE}" \
                cmake --build /work/build -j"$(nproc)" "$@" ;;
  test)       ensure_submodules
              podman run "${run_args[@]}" "${IMAGE}" \
                ctest --test-dir /work/build --output-on-failure "$@" ;;
  shell)      podman run -it "${run_args[@]}" "${IMAGE}" ;;
  run)        ensure_submodules
              podman run "${run_args[@]}" "${IMAGE}" /work/build/metro "$@" ;;
  vulkaninfo) podman run "${run_args[@]}" "${IMAGE}" vulkaninfo --summary ;;
  *)          usage; exit 1 ;;
esac
