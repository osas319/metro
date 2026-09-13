# Kadıköy — Referans ve Ölçüm Verisi (motor dışı)

Bu klasör **kaynak veridir**; motor tarafından okunmaz ve derlemeye girmez.
Amaç, her ölçünün nereden geldiğini izlenebilir tutmak — manuel modellemede
tek gerçek kaynak ölçümdür (`docs/MODELING.md` §12).

## Dosyalar

- `measurements.csv` — saha ölçüm defteri. Şablon hazır; sahada doldurulur.
  - `kaynak`: `lazer` | `serit` | `nivo` | `foto` | `ibb` | `tahmin`
  - `guven`: `measured` | `photo-scaled` | `estimated`
  - `estimated` satırlar modele alınmadan önce teyit edilir.
- Foto arşivi ve fotogrametri çıktıları bu klasörde **tutulmaz** (boyut ve
  lisans riski). Arşiv yolu, çekim tarihi ve lisans notu aşağıya yazılır.

## Arşiv kaydı

| Tarih | İçerik | Konum | Lisans/Not |
|---|---|---|---|
| — | (henüz arşiv yok) | — | — |

## Kaynak veri kalitesi

Ölçüm defterinde `guven = estimated` satır kaldığı sürece o elemanın
modeli **kesinleşmiş sayılmaz** ve `station.json`'a yazılmaz.
