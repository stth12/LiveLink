# Install release 0.1.0 / Установка версии 0.1.0

## English

Download files from [GitHub Releases](https://github.com/stth12/LiveLink/releases/tag/v0.1.0).

### Windows

Run `LiveLinkSetup-0.1.0.exe`. Keep the desktop, background process and VST3
components selected. Open Live Link and rescan VST3 plugins in your DAW.
The installer is not code-signed.

### iPhone — iOS 16 or newer

Download `LiveLink-0.1.0-unsigned.ipa`. The archive is intentionally unsigned:
Sideloadly or AltStore Classic signs it using your own Apple developer identity.
Follow the chosen tool's current setup instructions:

- [Sideloadly](https://sideloadly.io/): select the IPA and your connected iPhone,
  then follow its signing and installation flow.
- [AltStore Classic](https://faq.altstore.io/altstore-classic/your-altstore):
  configure AltServer and AltStore, then import the downloaded IPA from My Apps.

Enable Developer Mode and trust the developer profile if prompted by iOS.
Renew the app's signing before expiration as required by your account and tool.
These two specific installation workflows have not been independently tested
for this release; initial physical-iPhone playback has been confirmed by a user.

Connect the PC and iPhone to the same trusted local network. Allow local-network
access on iPhone, select your computer and enter the PIN shown on Windows.
Insert Live Link on the stereo master bus and start DAW playback.
USB audio transport is not implemented; a cable used for installation does not
enable USB streaming.

## Русский

Скачайте файлы из [релиза GitHub](https://github.com/stth12/LiveLink/releases/tag/v0.1.0).

### Windows

Запустите `LiveLinkSetup-0.1.0.exe`. Оставьте выбранными панель управления,
фоновый процесс и VST3. Откройте Live Link и обновите список VST3 в DAW.
Установщик не имеет цифровой подписи.

### iPhone — iOS 16 и новее

Скачайте `LiveLink-0.1.0-unsigned.ipa`. Файл подготовлен без подписи:
Sideloadly или AltStore Classic подписывает его с вашей учётной записью Apple.
Следуйте актуальной инструкции выбранного инструмента:

- [Sideloadly](https://sideloadly.io/): выберите IPA и подключённый iPhone,
  затем пройдите подписание и установку.
- [AltStore Classic](https://faq.altstore.io/altstore-classic/your-altstore):
  настройте AltServer и AltStore, затем импортируйте скачанный IPA из My Apps.

Включите режим разработчика и подтвердите доверие профилю, если этого потребует
iOS. Обновляйте подпись до истечения её срока в соответствии с правилами вашей
учётной записи и инструмента. Эти два способа установки отдельно не проверены
для данного релиза; пользователь уже подтвердил воспроизведение на реальном iPhone.

Подключите компьютер и iPhone к одной доверенной локальной сети. Разрешите
доступ к локальной сети, выберите компьютер и введите PIN из панели Windows.
Добавьте Live Link на стереомастер DAW и запустите воспроизведение.
Передача аудио по USB пока не реализована: кабель для установки приложения
не включает передачу звука по USB.
