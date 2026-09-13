# Metro geliştirme imajı — host'a hiçbir paket kurulmaz, tüm derleme bu
# imaj içinde çalışır (tools/container.sh ile kullanılır).
#
# Host ile uyumluluk notu: GPU erişimi /dev/dri üzerinden kernel DRM'e gider;
# Mesa (RADV) userspace imajın içindedir. Host Fedora 44 + AMD olduğu için
# sürücü/kernel uyumu garanti.
FROM registry.fedoraproject.org/fedora:44

RUN dnf -y --setopt=install_weak_deps=False install \
      gcc-c++ \
      cmake \
      ninja-build \
      git \
      ccache \
      glslc \
      glslang \
      libdecor \
      mesa-vulkan-drivers \
      SDL3-devel \
      vulkan-loader-devel \
      vulkan-tools \
      vulkan-validation-layers \
    && dnf clean all \
    && rm -rf /var/cache/dnf

WORKDIR /work
CMD ["/bin/bash"]
