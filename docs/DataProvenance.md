# Data provenance

## Исходный код

Опубликованный архив был предоставлен разработчиком Святославом Образцовым
(Suavik). Примерная дата архива `nw205PeopleWater.rar` — 18 января 1999 года.
Он не является release snapshot.

`License.txt` сообщает, что оригинальные файлы разработаны и предоставлены
Logos, и разрешает публикацию и использование исходных текстов программ и
данных с изменениями при обязательном указании происхождения Logos.

Проект сохраняет исходные credits и не приписывает современным участникам
авторство унаследованного кода или данных.

## Официальный retail-диск

Локально исследован образ:

```text
F:\Downloads\Русская рулетка II - Закрытые планеты\RR2.mdf
F:\Downloads\Русская рулетка II - Закрытые планеты\RR2.mds
```

Он был смонтирован как `G:`. Подтвержденные свойства:

- volume label `RR2`;
- CDFS;
- 6,782 файлов на всем диске;
- 6,768 файлов в `G:\nw`;
- дата записи filesystem 27 мая 1999 года;
- retail `nw.exe` размером 2,179,072 bytes;
- издательская и разработческая атрибуция Buka/Logos на обложке.

Образ, scans и retail files являются локальными preservation artifacts и не
должны автоматически коммититься или публиковаться.

## Текущая локальная установка

Исследован путь:

```text
E:\Games\The Next Worlds
```

Он не является чистым retail baseline:

- полный retail data set присутствует в корне;
- тот же data set продублирован в `E:\Games\The Next Worlds\nw`;
- добавлен dgVoodoo 2.73;
- изменены `game.cfg`, user config, saves и один briefing;
- registry `HDDir` и `CDDir` указывают на установленную директорию;
- для `nw.exe` задан AppCompat `HIGHDPIAWARE WINXPSP2`.

A focused 2026-07-26 comparison confirmed that all nine installed root
`Level.*\LEVEL.CFG` files are byte-identical to their `G:\nw` retail-CD
counterparts. The installed root `game.cfg` differs from the CD copy only in
`Init/StartLevel` (`3` locally versus retail `0`). This makes the Level configs
a trustworthy read-only runtime fixture while leaving `game.cfg` classified
as user-modified state.

The same date's recovered asset sweep opened, validated and released the
selected `.sce` header, `default.ptp` and shared `figs5x3c.fnt` for all nine
Levels in both `E:\Games\The Next Worlds` and `G:\nw`. The sweep performed no
writes to either data tree. This confirms compatibility of the bounded asset
bootstrap only; it does not yet claim successful object/terrain scene decoding.

On 2026-07-27 the recovered terrain sweep additionally validated, constructed
and released the real software terrain for all nine installed Levels in Debug
and Release. It read the five terrain sprites and five 8-bit masks in each Level
without modifying them. The resulting 512x512 height-map checksums agree across
configurations and identify seven distinct maps; this is terrain-decoder
evidence only and does not yet claim land-map or complete scene ownership.

The same date's scene-order sweep read and released every installed `1.sce` in
Debug and Release without modifying the Level trees. It validated named
references, order topology and land-object owner/copy indices, then executed the
historical land-map reader against the recovered terrain. The two configurations
produce identical summaries. Three scenes (`Level.01D`, `Level.01N` and
`Level.06N`) contain one valid empty trailing copy-index primary sentinel;
`Level.07N` contains 2,056 land primary rows and zero nested map entries. This is
structural scene evidence only: the resulting owner is deliberately not a
drawable or published `CViewScene`.

Основной установленный `nw.exe` совпадает с дисковым по SHA-256. Установка
полезна как user-state fixture, но не как clean retail fixture.

## Taxi attribute source identities

The 2026-07-28 read-only sweep found every selected May `SCINC/TAXI.SCI`
byte-identical between installed `E:\Games\The Next Worlds` and mounted
`G:\nw`. Seven SHA-256 groups cover the nine Levels:

- Level.01D/01N: `0FF1A02150DB84C2765D616D35A7835478732DBDAD4DEC5240DAF9D8669EA3A7`;
- Level.02D/02N: `115848E1742E547F9B3E6C29B008A5EEA476F7F39304DAB6244A2350BAC8BB21`;
- Level.03N: `A0D38B22334B558DCC3A62A4C6B53BBBD3CE419309638B548A7311B443ECEBC4`;
- Level.04D: `8EF98BE9CCC797D03FBCDAE89FEDEBD4D5712CB1EE313868CBE8E9F9B23BD07B`;
- Level.05D: `685A780CCBCD51458112BAB62E6A5132F21F751BF2E9423E44C337CAAAA9EB60`;
- Level.06N: `5981BFE10F5F39FF5778752606FA78702573A0B2906A64C65BA590FBA37DB058`;
- Level.07N: `2E6B6898483E8C48BF06812312B42D4CC94807CB5BB69EDC4830127F204987B9`.

The public repository fixture is a distinct January Level.03N source,
`0A2037173379376C3B38E3FCC051FF51286D4793999F5A5CEF02895A7EB9F10A`.
It is admitted only for hermetic CI and is never described as May content.

## Vehicle attribute source identities

The 2026-07-28 read-only sweep likewise found every selected May
`SCINC/VEHICLE.SCI` byte-identical between installed
`E:\Games\The Next Worlds` and mounted `G:\nw`. Seven SHA-256 groups cover the
nine Levels:

- Level.01D/01N: `E9AAB05DA2E2F44F45E657F192265FD8095B90856A1D630C6373FB138A23A92A`;
- Level.02D/02N: `A48D64915904031741807DF5D21DFBBF6163DCC14BB8ADDF64B428D4B2A158E2`;
- Level.03N: `A4FFF3DC84856AB664CED63E4E801D48B8CB01B981B21FB908AB28409C2396B0`;
- Level.04D: `EE42C8B64E19E21FD96C5FC2EEC80FA76D0B4CC76900451405474D1A618325F7`;
- Level.05D: `664D7E7FB80D737DE7354BECBF8E0AC37B0D68B6F3C00C8049F39AA6397970DE`;
- Level.06N: `356664FEFFBA3D96E1E503D37D4177DF6155B0574A4E4FD574A48765B3AA6AA7`;
- Level.07N: `E83751756B62F4199A313B85C446D2865CC9A62BE7611C1E7546B0DF9A123E95`.

The public repository's January Level.03N Vehicle fixture is distinct:
`8ACFBA97C9D6F1CD658647AB12847394647251507E16AEDC463165A9BFA8BEC9`.
It is used only by the hermetic source/rollback test and has a `3/8` roster;
the May Level.03N source has `7/7`. No retail source is copied into Git.

## Bullet attribute source identities

The 2026-07-28 read-only sweep found retail root `BULLET.SCI` and every
selected May `SCINC/bullet_loc.sci` byte-identical between installed
`E:\Games\The Next Worlds` and the preserved mounted-disc fixture. Root
`BULLET.SCI` is
`0A418F00ED079DD719572980B610B838F7EE9F44A37A9E3D7B06530C30639403`.
Seven local SHA-256 groups cover the nine Levels:

- Level.01D/01N: `1430F7B850A9A8248241D19E115432D99EC26F783C8C4D47392C7E603731715B`;
- Level.02D/02N: `1FF38B5B85F5C2AAB66025C1B6811599FFBA999A138C31F66FE068B69C0ABCA5`;
- Level.03N: `011EB3FE765872B6FC122339BBA67C728199C9F2432A709106EF41E74B66A930`;
- Level.04D: `62CEB31A8B7197177050AAB9B7E99A1753B283ABCEF4BDFC435AAD8EC2B3B25B`;
- Level.05D: `2BD5472DB2F6B452ADDF3D525076B60C9D04E2CB9C979D9019938FA5A5BF1867`;
- Level.06N: `B7C62E7CFFD56B354BBF94396A172A883C1FBC23603EDE0F06E77B44C1BD6460`;
- Level.07N: `396CBA32A520F689302B8AFDDB75D52F77014C30B5E3385259746755644CA6EB`.

The repository's January root/local fixtures are distinct:
`F4554E35C66540B55379135D5313B00093EF32BE1B44F9BB93476D93EB349E62`
and
`260F0F8FE4AAAC94B71EF5FB217577C9E3E886E25260E4F5195230CE29C44EF6`.
They are admitted only as a hermetic `4/4` source and rollback fixture. No May
Bullet script is copied into public Git.

## User-generated/private artifacts

В установке обнаружены пользовательские `save0` и `save1`. Они полезны для
legacy save importer и regression tests, но по умолчанию остаются приватными.
В публичный test corpus может попасть только специально созданный или явно
разрешенный sanitized fixture.

Registry exports, WER reports и dumps также являются приватными, даже когда
они используются для диагностики.

## External patch candidate

`NWRUS.EXE` находится рядом с образом, но не установлен вместо retail EXE. Он:

- имеет размер 797,286 bytes;
- упакован UPX 0.59;
- имеет PE timestamp, не отражающий реальную дату;
- не сопровождается достаточным changelog/provenance.

До распаковки и сравнения он классифицируется как `UNKNOWN_EXTERNAL_PATCH`, а
не как release baseline.

## Third-party components

Оригинальный диск содержит:

- DirectX 5-era installer;
- Intel RSX libraries и setup tools;
- Buka/installer components.

Они не нужны современному порту и не должны включаться в release package.
Разрешение Logos на исходники/данные не следует автоматически трактовать как
разрешение на распространение всех third-party binaries.

## Publication classes

| Class | Public Git | Release package |
| --- | --- | --- |
| Original source under project license | Yes, with attribution | Yes |
| Modern source/tests/docs | Yes | Source as selected |
| Hash manifests without media | Yes | Optional |
| Retail scripts/data | After scope review | Prefer importer first |
| Models/textures/audio/video from CD | No by default | Import locally |
| DirectX/RSX installers | No | No |
| Retail EXE | No | No |
| User saves/dumps/registry | No | No |
| Example mod created for RR2NW | Yes | Yes |

## Required attribution

Каждый source и binary release должен:

- сохранять `License.txt`;
- указывать Logos как разработчика оригинальных файлов;
- указывать Buka как издателя оригинального русского retail-издания;
- отделять original credits от RR2NW contributors;
- перечислять licenses новых dependencies и assets.
