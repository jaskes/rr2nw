# Документация RR2NW

Документация отделяет исторические материалы, подтвержденные эталоны и планы
современного продолжения игры. Исходный код и тесты остаются главным источником
истины для реализованного поведения; документы фиксируют продуктовые решения,
доказательства и критерии приемки.

## Основные документы

- [Roadmap.md](Roadmap.md) — Windows-first план до версии 1.0.
- [Architecture.md](Architecture.md) — целевая архитектура и порядок
  осовременивания без большого переписывания.
- [BuildPortAudit.md](BuildPortAudit.md) — измеренный legacy build graph,
  compiler blockers и порядок подключения модулей к CMake.
- [RetailParity.md](RetailParity.md) — измеренный разрыв между январским
  snapshot и официальным retail-диском.
- [CompatibilityLedger.md](CompatibilityLedger.md) — живой реестр странностей
  legacy-кода, retail-данных и сериализаторов с доказательствами, текущей
  обработкой и условиями пересмотра.
- [PlayerVehicleReadiness.md](PlayerVehicleReadiness.md) — current evidence,
  ownership gaps and the shortest gated path from the recovery observer to a
  real drivable `Vehicle.Default`.
- [ManualAcceptance.md](ManualAcceptance.md) — commands and evidence contract
  for selecting, automatically sweeping and manually checking all retail
  Levels on Windows.
- [BehaviorDecisions.md](BehaviorDecisions.md) — принятые решения там, где
  исторические источники или возможные реализации расходятся.

## Эталоны и происхождение данных

- [ReferenceBuild.md](ReferenceBuild.md) — воспроизводимые reference-артефакты,
  автоматические проверки и необязательная legacy-сборка.
- [DataProvenance.md](DataProvenance.md) — происхождение диска, локальной
  установки, исходников и границы публикации данных.
- [../reference/reports/README.md](../reference/reports/README.md) — принятые
  агрегированные результаты M0 без retail payload и локальных путей.

## Выпуск

- [ReleaseProcess.md](ReleaseProcess.md) — ветки, версии, release gate,
  публикация и hotfix-процесс.
- [../CHANGELOG.md](../CHANGELOG.md) — уже выполненные изменения современного
  продолжения.

Локальный `WINDOWS_AGENT_HANDOFF.md` предназначен для текущего состояния
рабочей машины и не является публичной историей проекта. Диагностика, дампы,
приватные retail-данные и сохранения также не должны попадать в релизный Git.

## Replay journal

- [ReplayJournal.md](ReplayJournal.md) documents the CTJ1
  normalized-control format, focus/held-action semantics, checkpoint contract,
  local Vehicle replay proof and current limits.
- [LevelContinuation.md](LevelContinuation.md) documents the LCN1 container,
  stable capture boundary, fresh-session transactional restore, whole-world
  recapture proof and resumed Vehicle control journal.
- [SaveSlots.md](SaveSlots.md) documents the eight fixed RR2SLOT1 files,
  bounded metadata/preview envelope, native Windows menu broker, per-user save
  root, atomic replacement and destroyed-Level disk-load proof.
