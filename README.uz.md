[English](README.md) · [Русский](README.ru.md) · **O‘zbek**

# AetherWall

**Windows 11 uchun jonli fon rasmlari: GPU sahnalari, bir nechta monitor va animatsiyani boshqarish.**

AetherWall Direct3D 11 va HLSL yordamida sahnalarni ish stoli belgilarining orqasida
chizadi. Fon rasmini tanlang, har bir monitorga alohida sahna belgilang va animatsiya
qachon to‘xtatilishini sozlang. Manager oynasini yopsangiz ham fon ishlashda davom
etadi — mustaqil Core tizim treyida qoladi.

[![CI](https://github.com/Ordo-xsenos/open_wallpaper/actions/workflows/release-preview.yml/badge.svg)](https://github.com/Ordo-xsenos/open_wallpaper/actions/workflows/release-preview.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Windows 11 x64](https://img.shields.io/badge/Windows-11%20x64-0078D4)
![Preview](https://img.shields.io/badge/Status-Preview-orange)

[Windows uchun yuklab olish](https://github.com/Ordo-xsenos/open_wallpaper/releases) ·
[CI yig‘ilmalari](https://github.com/Ordo-xsenos/open_wallpaper/actions/workflows/release-preview.yml) ·
[Muammo haqida xabar berish](https://github.com/Ordo-xsenos/open_wallpaper/issues/new/choose)

![AetherWall sahnalar katalogi](docs/screenshots/m5-discover-linux.png)

*Linux ishlab chiqish muhitida olingan haqiqiy Manager skrinshoti. Oynada sinov
ulanishi belgilangan; bu rasm Windows ish stolida chizishni namoyish qilmaydi.*

## Imkoniyatlar

- **GPU sahnalari.** Rasm qatlamlari, gradientlar, harakat va zarralar: yulduzlar,
  yomg‘ir va qor. Ilovaga oltita original namuna kiritilgan.
- **Bir nechta monitor.** Barcha ekranlarga bitta sahna yoki har biriga alohida
  sahna belgilang. Ekran joylashuvi va DPI hisobga olinadi.
- **Animatsiya boshqaruvi.** 15/30/60 FPS chegarasi, GPU tanlash va to‘liq ekranli
  ilova, batareya yoki qulflangan seans uchun pauza qoidalari.
- **Mahalliy kutubxona.** Sahna papkalari va `.awscene` paketlarini import qilish,
  qidirish, sevimlilar va monitorlarga berilgan sahnalarni qayta tiklash.
- **Katalog.** Qidiruv, toifalar, yuklab olishni bekor qilish va SHA-256 tekshiruvi.
  O‘rnatilgan fonlar va ilovaga kiritilgan namunalar internetsiz ishlaydi.
- **Ruscha va inglizcha interfeys.** Qorong‘i Qt Manager, trey boshqaruvi va
  Windows tizimiga kirishda ixtiyoriy avtomatik ishga tushirish.

## Yuklab olish va o‘rnatish

**Windows 11 24H2 yoki yangiroq (build 26100+), x64** va Direct3D 11 qo‘llaydigan
videokarta kerak. Tayyor yig‘ilma uchun Visual Studio, Qt yoki Python kerak emas.

[Releases](https://github.com/Ordo-xsenos/open_wallpaper/releases) sahifasida
mavjud sinov yig‘ilmasini tanlang va **Assets** bo‘limini oching:

| Fayl | Vazifasi |
| --- | --- |
| `AetherWall-…-win-x64-setup.exe` | O‘rnatuvchi — odatiy o‘rnatish usuli |
| `AetherWall-…-win-x64.zip` | O‘rnatmasdan ishlatish; ZIP faylini to‘liq oching |
| `SHA256SUMS.txt` va `*.sha256` | Yuklab olingan fayllarning nazorat summalari |
| `AetherWall-M5.zip` | Loyiha manba kodi, tayyor ilova emas |
| `qt…-source.zip` | Qt kutubxonalarining tegishli manba kodlari |

O‘rnatuvchi administrator huquqini talab qilmaydi va ilovani
`%LOCALAPPDATA%\Programs\AetherWall` papkasiga joylaydi. Avtomatik ishga tushirishni
sozlamalarda alohida yoqing. ZIP variantida `AetherWall.Manager.exe` faylini ishga
tushiring; barcha DLL va papkalarni yonida saqlang. Har ikkala variant ham
sozlamalar va kutubxonani foydalanuvchi profilida saqlaydi.

## Birinchi ishga tushirish

1. **AetherWall** ilovasini oching va **Start engine** tugmasini bosing.
2. **Library** bo‘limida sahna va monitorni tanlab, qo‘llang.
3. Unumdorlik sozlamalarida FPS va pauza qoidalarini belgilang.

Manager oynasi yopilganda fonlar ishlashda davom etadi. Barchasini to‘xtatish
uchun ilova yoki trey menyusidagi to‘liq chiqish buyrug‘idan foydalaning.
Yangilash yoki o‘chirishdan oldin to‘liq chiqing. Ilovani o‘chirish kutubxona,
sozlamalar va import qilingan manba fayllarini saqlab qoladi.

Oddiy oynada chizishni tekshirish uchun `AetherWall.Manager.exe --core-window`
buyrug‘ini ishga tushiring, keyin dvigatelni yoqing. Bu diagnostika rejimi GPU
muammolarini ish stoli integratsiyasi muammolaridan ajratishga yordam beradi.

## O‘z sahnalaringiz va katalog

Sahna `manifest.json`, rasmlar va ichki effektlardan iborat. Papka yoki `.awscene`
paketini Library orqali import qiling. [Sahna formati](docs/SCENE_FORMAT.md) va
[namunalar](scenes/README.md) bilan tanishing.

Standart holatda ommaviy katalog serveri ko‘rsatilmagan. **Discover → Examples**
bo‘limida ichki namunalarni ochish mumkin. O‘z katalogingiz uchun sozlamalarda
HTTPS manzilini kiriting. Alohida [FastAPI serveri](server/README.md) qidiruv,
moderatsiya, mahalliy yoki S3 bilan mos saqlash tizimini qo‘llaydi. Kontentni
nashr qilish uchun muallif huquqi yoki ruxsati bo‘lishi kerak.

## Loyiha holati

AetherWall hozir **preview** bosqichida. CI kompilyatsiya va avtomatik testlarni
tekshiradi; Windows, GPU va monitor konfiguratsiyalarining to‘liq moslik jadvali
hali tekshirilishi kerak. Windows qobig‘i bilan integratsiya tajribaviy.
Unumdorlik ko‘rsatkichlari tasdiqlanmagan. Hozirgi yig‘ilmalar raqamli imzosiz.

`main` tarmog‘idagi muvaffaqiyatli yig‘ilma o‘rnatuvchi, ZIP, manba kodlari va
nazorat summalari bilan alohida **Pre-release** yaratadi. Xatoli yig‘ilmalar va
pull request nashr qilinmaydi. Har bir nashr o‘z kommitiga bog‘langan va avvalgi
fayllarni almashtirmaydi.

Muammo yuz bersa, yig‘ilma versiyasi, Windows, GPU/drayver, takrorlash qadamlari
va shaxsiy ma’lumotlar olib tashlangan `%LOCALAPPDATA%\AetherWall\logs` jurnallarini
ilova qiling. [Qabul sinovlari](docs/M5_TEST_PLAN.md) va
[ish stoli mosligi](docs/DESKTOP_INTEGRATION.md) haqida o‘qing.

## Ishlab chiqish

C++20, Direct3D 11/HLSL, Qt Quick va SQLite. Core chizish, doimiy holat va treyni
boshqaradi. Manager Named Pipe orqali ulanadi va tarmoq so‘rovlarini bajaradi.
Python katalog serveri alohida ishga tushiriladi.

- [Yig‘ish va diagnostika](docs/DEVELOPMENT.md)
- [O‘rnatuvchini yig‘ish va nashrlar](docs/RELEASE.md)
- [Sahna formati](docs/SCENE_FORMAT.md) va [IPC protokoli](docs/IPC.md)
- [Amalga oshirish holati](docs/IMPLEMENTATION_STATUS.md) va [spetsifikatsiya](WINDOWS11_WALLPAPER_ENGINE_SPEC.md)

## Hissa qo‘shish

[CONTRIBUTING](CONTRIBUTING.md) va [xulq-atvor qoidalarini](CODE_OF_CONDUCT.md) o‘qing.
Xatolar va takliflar uchun [Issues shakllaridan](https://github.com/Ordo-xsenos/open_wallpaper/issues/new/choose)
foydalaning. Xavfsizlik zaifliklarini [SECURITY](SECURITY.md) bo‘yicha maxfiy yuboring.

## Litsenziya

Loyiha kodi — [MIT](LICENSE). Original namunalar — CC0-1.0.
Bog‘liqliklarning o‘z shartlari bor: [uchinchi tomon litsenziyalari](THIRD_PARTY_NOTICES.md).
