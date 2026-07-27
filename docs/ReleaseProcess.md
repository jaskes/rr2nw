# Release process

## Ветки

Workflow использует две постоянные ветки:

- `develop` — integration branch для gameplay, platform, content и tooling;
- `master` — опубликованная release history; обычная разработка в ней не
  ведется.

До первого опубликованного milestone GitHub default branch — `develop`: она
показывает актуальное интеграционное состояние и делает активную историю
проекта основной. При первом продвижении полного `develop` в `master` и
создании release tag default branch переключается на `master`, как в Four
Winds Reborn.

Публичные pull requests не являются частью принятого release-процесса. После
прохождения gate владелец напрямую сливает полный `develop` в `master` явным
release commit, как в Four Winds Reborn.

Remotes должны иметь однозначные роли:

- `origin` должен указывать на `jaskes/rr2nw`;
- `upstream` должен указывать на `Marisa-Chan/rr2nw`.

В `upstream` проект никогда не выполняет push. Permanent branches и
опубликованные tags не force-push и не переписываются.

## Versioning

Используется SemVer `vMAJOR.MINOR.PATCH`.

- MINOR до 1.0 закрывает законченный roadmap milestone.
- PATCH исправляет совместимую ошибку уже опубликованного milestone.
- `v1.0.0` резервируется для принятого Windows core roadmap.
- Build metadata включает revision, compiler и dirty marker для local builds.

Рекомендуемые checkpoints:

- `v0.0.1` — automated evidence/manifests;
- `v0.1.0` — modern Windows x86 vertical slice;
- `v0.2.0` — SDL platform/stability;
- `v0.5.0` — retail parity;
- `v0.7.0` — save/replay/VFS;
- `v0.9.0` — mods and beta;
- `v1.0.0-rc.N` — exact release candidates;
- `v1.0.0` — accepted Windows release.

Номера могут меняться до публикации, но tag и project version должны совпадать.

## Normal development

- Одно изменение поведения или один mechanical refactor на reviewable commit.
- Не смешивать перенос файлов с gameplay change.
- Не создавать абстракцию без ближайшего consumer.
- `win10-stability-fixes` принимается после modern/legacy build evidence и
  targeted tests; опубликованную ветку не требуется переписывать.
- Решение retail/behavior conflict обновляет `RetailParity.md` или
  `BehaviorDecisions.md` в том же change.

Обычный цикл:

```powershell
git switch develop
git pull --ff-only origin develop
# edit, test, commit
git push origin develop
```

Windows CI запускается прямыми pushes в обе постоянные ветки; обязательный PR
для запуска CI не нужен.

## Milestone gate до RC

Каждый milestone требует:

1. Clean Windows configure/build.
2. Focused automated tests.
3. Time-bounded launch smoke, если milestone затрагивает runtime.
4. Отсутствие новых WER events/crash dumps в smoke.
5. Обновленные docs/contracts.
6. `CHANGELOG.md` только для принятого результата.
7. Проверку, что retail data, saves, dumps и handoff не staged.

M0 и M1 не требуют ручного прохождения. Неблокирующий developer smoke можно
выполнять по желанию.

## Release candidate gate

Перед RC scope замораживается. Обязательные автоматические проверки:

1. Clean MSVC Debug, Release и sanitizer builds.
2. Все unit/integration/content tests.
3. Boot smoke всех девяти runtime directories.
4. Save round-trip и legacy importer fixtures.
5. Fixed-seed replay/state-hash verification.
6. Base content и example mod validation.
7. Packaging из clean staging directory.
8. Проверка GUI subsystem и PE mitigations.
9. SHA-256 для каждого публикуемого artifact.
10. Повторный запуск точного unpacked package на clean user profile.

После автоматического gate выполняется ручная матрица из
[Roadmap.md](Roadmap.md). Тестируется package, а не build-tree EXE.

## Packaging contract

Windows package содержит:

- RR2NW EXE и runtime dependencies;
- data importer и content validator;
- example mod;
- license/credits/changelog;
- symbols отдельно, если размер или приватность требуют отдельного artifact.

По умолчанию package не содержит retail EXE, DirectX/RSX installers, полный CD
data set, user saves или machine-local config.

Importer:

- принимает mounted original CD или существующую установку;
- проверяет известный manifest или формирует диагностический unknown-build
  report;
- исключает third-party installers и лишний nested duplicate;
- создает normalized local data pack;
- не изменяет источник;
- пишет отчет с baseline/content identity.

## Tag and publish

После принятия RC полный `develop` напрямую продвигается в `master`. PR для
этого не создается. Для примера ниже публикуется `v0.1.0`:

```powershell
git switch master
git pull --ff-only origin master
git merge --no-ff develop -m "release: v0.1.0"
git push origin master
git tag -a v0.1.0 -m "RR2NW v0.1.0"
git push origin v0.1.0
```

Порядок обязателен: сначала `master`, затем tag. CI релиза должен отклонять
tag, если tagged commit отсутствует в `origin/master` или версия tag не
совпадает с project version.

После публикации независимо скачиваются и проверяются archives и SHA-256,
затем рабочая копия возвращается на `develop`. Если release merge содержит
release-only изменение, `master` сразу reconciled обратно в `develop`.

Публикация считается завершенной только после проверки скачанного artifact.

## Hotfix

Hotfix создается от опубликованного `master` и меняет минимальный scope. Он
проходит affected tests и применимые части полного release gate. После PATCH
release исправление обязательно возвращается в `develop`:

```powershell
git switch develop
git pull --ff-only origin develop
git merge --no-ff master -m "reconcile hotfix v0.1.1"
git push origin develop
```

## Запрещенные release artifacts

- MDF/MDS/ISO и извлеченные retail assets без отдельного решения;
- retail/patch EXE;
- local saves, settings и registry exports;
- crash dumps, WER archives и diagnostics;
- build trees и compiler caches;
- credentials;
- `WINDOWS_AGENT_HANDOFF.md`.
