#pragma once

enum Lang { LANG_EN, LANG_PT, LANG_ES, LANG_DE, LANG_COUNT };

struct Str { const wchar_t *t[LANG_COUNT]; };

extern Lang g_lang;
inline const wchar_t *tr(const Str &s) { return s.t[g_lang]; }

namespace txt {

static const Str lang_name[LANG_COUNT] = {
    {{L"English", L"English", L"English", L"English"}},
    {{L"Português (Brasil)", L"Português (Brasil)", L"Português (Brasil)", L"Português (Brasil)"}},
    {{L"Español", L"Español", L"Español", L"Español"}},
    {{L"Deutsch", L"Deutsch", L"Deutsch", L"Deutsch"}},
};
static const char *const lang_code[LANG_COUNT] = {"en", "pt", "es", "de"};

static const Str choose_lang = {{L"Choose your language", L"Escolha seu idioma", L"Elige tu idioma", L"Wähle deine Sprache"}};
static const Str choose_lang_sub = {{L"You can change it later by running the installer again.",
                                     L"Dá para trocar depois, é só abrir o instalador de novo.",
                                     L"Puedes cambiarlo después abriendo el instalador de nuevo.",
                                     L"Du kannst sie später ändern, indem du den Installer erneut startest."}};

static const Str step_lang = {{L"Language", L"Idioma", L"Idioma", L"Sprache"}};
static const Str step_mods = {{L"Mods", L"Mods", L"Mods", L"Mods"}};
static const Str step_games = {{L"Games", L"Jogos", L"Juegos", L"Spiele"}};
static const Str step_install = {{L"Install", L"Instalar", L"Instalar", L"Installation"}};

static const Str hero_title = {{L"Feel the truck, not the keyboard.", L"Sinta o caminhão, não o teclado.",
                                L"Siente el camión, no el teclado.", L"Spür den Truck, nicht die Tastatur."}};
static const Str hero_sub = {{L"Plugins that change how Euro Truck Simulator 2 and American Truck Simulator feel on keyboard and mouse.",
                              L"Plugins que mudam a forma de jogar Euro Truck Simulator 2 e American Truck Simulator no teclado e mouse.",
                              L"Plugins que cambian la forma de jugar Euro Truck Simulator 2 y American Truck Simulator con teclado y ratón.",
                              L"Plugins, die verändern, wie sich Euro Truck Simulator 2 und American Truck Simulator mit Tastatur und Maus anfühlen."}};

static const Str drive_tag = {{L"Steering, pedals, weight and grip on the keyboard",
                               L"Direção, pedais, peso e aderência no teclado",
                               L"Dirección, pedales, peso y agarre en el teclado",
                               L"Lenkung, Pedale, Gewicht und Grip auf der Tastatur"}};
static const Str drive_b1 = {{L"W A S D become a real wheel and pedals: smooth, analog, with weight",
                              L"W A S D viram volante e pedais de verdade: suaves, analógicos, com peso",
                              L"W A S D se convierten en volante y pedales reales: suaves, analógicos, con peso",
                              L"W A S D werden zu echtem Lenkrad und Pedalen: sanft, analog, mit Gewicht"}};
static const Str drive_b2 = {{L"Empty truck feels light, heavy loads feel heavy",
                              L"Vazio fica leve, carregado fica pesado",
                              L"Vacío se siente ligero, cargado se siente pesado",
                              L"Leer fühlt er sich leicht an, beladen schwer"}};
static const Str drive_b3 = {{L"The wheel goes light on ice, snow, mud and wet roads",
                              L"O volante fica leve no gelo, neve, barro e chuva",
                              L"El volante se aligera en hielo, nieve, barro y lluvia",
                              L"Das Lenkrad wird leicht auf Eis, Schnee, Schlamm und nasser Straße"}};

static const Str view_tag = {{L"A real driver's head and eyes inside the cab",
                              L"Cabeça e olhar de motorista de verdade na cabine",
                              L"La cabeza y la mirada de un conductor real en la cabina",
                              L"Kopf und Blick eines echten Fahrers in der Kabine"}};
static const Str view_b1 = {{L"Mouse look with weight and inertia, head zoom on the middle button",
                             L"Mouse com peso e inércia, zoom de cabeça no botão do meio",
                             L"Ratón con peso e inercia, zoom de cabeza con el botón central",
                             L"Mausblick mit Gewicht und Trägheit, Kopf-Zoom mit der mittleren Taste"}};
static const Str view_b2 = {{L"Steady gaze: your neck compensates the cab's sway",
                             L"Olhar estabilizado: o pescoço compensa o balanço da cabine",
                             L"Mirada estable: el cuello compensa el balanceo de la cabina",
                             L"Ruhiger Blick: der Nacken gleicht das Schwanken der Kabine aus"}};
static const Str view_b3 = {{L"Body inertia, breathing and subtle surface changes, no camera shake",
                             L"Inércia do corpo, respiração e troca de piso sutil, sem tremida",
                             L"Inercia del cuerpo, respiración y cambios de superficie sutiles, sin temblores",
                             L"Körperträgheit, Atmung und sanfte Untergrundwechsel, ohne Kamerawackeln"}};

static const Str also_available = {{L"Also available", L"Também disponível", L"También disponible", L"Ebenfalls erhältlich"}};
static const Str get_it = {{L"Get it at mods.sznt.dev", L"Baixe em mods.sznt.dev", L"Descárgalo en mods.sznt.dev", L"Hol es dir auf mods.sznt.dev"}};

static const Str games_title = {{L"Where should we install?", L"Onde vamos instalar?", L"¿Dónde lo instalamos?", L"Wo sollen wir installieren?"}};
static const Str games_sub = {{L"We found these games on your PC. Close the game before continuing.",
                               L"Encontramos estes jogos no seu PC. Feche o jogo antes de continuar.",
                               L"Encontramos estos juegos en tu PC. Cierra el juego antes de continuar.",
                               L"Wir haben diese Spiele auf deinem PC gefunden. Schließe das Spiel, bevor du fortfährst."}};
static const Str no_games = {{L"No game found automatically. Add the game folder below.",
                              L"Nenhum jogo encontrado automaticamente. Adicione a pasta do jogo abaixo.",
                              L"No se encontró ningún juego. Añade la carpeta del juego abajo.",
                              L"Kein Spiel automatisch gefunden. Füge den Spielordner unten hinzu."}};
static const Str add_folder = {{L"+  Add game folder", L"+  Adicionar pasta do jogo", L"+  Añadir carpeta del juego", L"+  Spielordner hinzufügen"}};
static const Str folder_title = {{L"Choose the game folder (the one that contains \"bin\")", L"Escolha a pasta do jogo (a que tem a pasta \"bin\")",
                                  L"Elige la carpeta del juego (la que contiene \"bin\")", L"Wähle den Spielordner (der \"bin\" enthält)"}};
static const Str folder_bad = {{L"eurotrucks2.exe or amtrucks.exe was not found in bin\\win_x64 of that folder.",
                                L"Não encontrei eurotrucks2.exe nem amtrucks.exe em bin\\win_x64 dessa pasta.",
                                L"No se encontró eurotrucks2.exe ni amtrucks.exe en bin\\win_x64 de esa carpeta.",
                                L"eurotrucks2.exe oder amtrucks.exe wurde in bin\\win_x64 dieses Ordners nicht gefunden."}};
static const Str st_not_installed = {{L"Not installed", L"Não instalado", L"No instalado", L"Nicht installiert"}};
static const Str st_installed = {{L"Installed", L"Instalado", L"Instalado", L"Installiert"}};
static const Str st_old = {{L"Old version - will be upgraded", L"Versão antiga - será atualizada", L"Versión antigua - se actualizará", L"Alte Version - wird aktualisiert"}};
static const Str st_running = {{L"Game is running - close it", L"Jogo aberto - feche antes", L"Juego abierto - ciérralo", L"Spiel läuft - bitte schließen"}};

static const Str btn_next = {{L"Next", L"Continuar", L"Continuar", L"Weiter"}};
static const Str btn_back = {{L"Back", L"Voltar", L"Atrás", L"Zurück"}};
static const Str btn_install = {{L"Install", L"Instalar", L"Instalar", L"Installieren"}};
static const Str btn_update = {{L"Update", L"Atualizar", L"Actualizar", L"Aktualisieren"}};
static const Str btn_uninstall = {{L"Uninstall", L"Desinstalar", L"Desinstalar", L"Deinstallieren"}};
static const Str btn_finish = {{L"Finish", L"Concluir", L"Finalizar", L"Fertig"}};
static const Str btn_launch = {{L"Launch game", L"Abrir o jogo", L"Abrir el juego", L"Spiel starten"}};
static const Str btn_download = {{L"Download", L"Baixar", L"Descargar", L"Herunterladen"}};
static const Str btn_retry_admin = {{L"Retry as administrator", L"Tentar como administrador", L"Reintentar como administrador", L"Als Administrator erneut versuchen"}};

static const Str installing = {{L"Installing...", L"Instalando...", L"Instalando...", L"Wird installiert..."}};
static const Str removing = {{L"Removing...", L"Removendo...", L"Eliminando...", L"Wird entfernt..."}};
static const Str done_title = {{L"All set!", L"Tudo pronto!", L"¡Todo listo!", L"Alles bereit!"}};
static const Str done_sub = {{L"Start the game. It will warn about \"advanced SDK features\": that's normal, click OK.",
                              L"Abra o jogo. Ele vai avisar sobre \"recursos avançados do SDK\": é normal, clique em OK.",
                              L"Abre el juego. Avisará sobre \"funciones avanzadas del SDK\": es normal, haz clic en Aceptar.",
                              L"Starte das Spiel. Es warnt vor \"erweiterten SDK-Funktionen\": das ist normal, klicke auf OK."}};
static const Str done_warn_title = {{L"Finished with warnings", L"Terminou com avisos", L"Terminado con avisos", L"Mit Warnungen abgeschlossen"}};
static const Str removed_title = {{L"Removed", L"Removido", L"Eliminado", L"Entfernt"}};
static const Str removed_sub = {{L"Your settings were kept (.ini.bak) in case you come back.",
                                 L"Seus ajustes ficaram guardados (.ini.bak) caso volte a instalar.",
                                 L"Tus ajustes se guardaron (.ini.bak) por si vuelves a instalar.",
                                 L"Deine Einstellungen wurden gesichert (.ini.bak), falls du zurückkommst."}};
static const Str admin_needed = {{L"Windows blocked writing to the game folder.", L"O Windows bloqueou a gravação na pasta do jogo.",
                                  L"Windows bloqueó la escritura en la carpeta del juego.", L"Windows hat das Schreiben in den Spielordner blockiert."}};

static const Str controls_title = {{L"Quick controls", L"Controles rápidos", L"Controles rápidos", L"Schnellsteuerung"}};
static const Str ctl_ws = {{L"Throttle and brake, progressive", L"Acelerador e freio, progressivos", L"Acelerador y freno, progresivos", L"Gas und Bremse, progressiv"}};
static const Str ctl_ad = {{L"Steering with weight and self-centering", L"Volante com peso e volta ao centro", L"Volante con peso y autocentrado", L"Lenkung mit Gewicht und Rückstellung"}};
static const Str ctl_double = {{L"Tap twice and hold: floor it / emergency brake", L"Toque duplo e segure: pé fundo / freio de emergência",
                                L"Doble toque y mantén: a fondo / freno de emergencia", L"Zweimal tippen und halten: Vollgas / Notbremse"}};
static const Str ctl_mouse = {{L"Look around with weight and inertia", L"Olhar em volta com peso e inércia", L"Mirar alrededor con peso e inercia", L"Umsehen mit Gewicht und Trägheit"}};
static const Str ctl_middle = {{L"Hold to lean in (head zoom)", L"Segure para inclinar (zoom de cabeça)", L"Mantén para inclinarte (zoom de cabeza)", L"Halten zum Vorbeugen (Kopf-Zoom)"}};
static const Str ctl_ini = {{L"Every detail is adjustable in sznt-drive.ini / sznt-view.ini, in the game's plugins folder.",
                             L"Tudo é ajustável em sznt-drive.ini / sznt-view.ini, na pasta plugins do jogo.",
                             L"Todo se ajusta en sznt-drive.ini / sznt-view.ini, en la carpeta plugins del juego.",
                             L"Alles lässt sich in sznt-drive.ini / sznt-view.ini im plugins-Ordner des Spiels anpassen."}};

static const Str uninstall_title = {{L"Remove from your games", L"Remover dos seus jogos", L"Eliminar de tus juegos", L"Aus deinen Spielen entfernen"}};
static const Str uninstall_sub = {{L"The plugin files are deleted and the game controls go back to how they were.",
                                   L"Os arquivos do plugin são apagados e os controles do jogo voltam a ser como eram.",
                                   L"Se borran los archivos del plugin y los controles del juego vuelven a como estaban.",
                                   L"Die Plugin-Dateien werden gelöscht und die Spielsteuerung wird wiederhergestellt."}};

static const Str update_available = {{L"Version %ls is available", L"A versão %ls está disponível", L"La versión %ls está disponible", L"Version %ls ist verfügbar"}};

// installer messages
static const Str m_old_disabled = {{L"Old version disabled: %ls", L"Versão antiga desativada: %ls", L"Versión antigua desactivada: %ls", L"Alte Version deaktiviert: %ls"}};
static const Str m_installed = {{L"%ls %ls installed", L"%ls %ls instalado", L"%ls %ls instalado", L"%ls %ls installiert"}};
static const Str m_kept = {{L"%ls %ls installed - your settings were kept", L"%ls %ls instalado - seus ajustes foram mantidos",
                            L"%ls %ls instalado - se conservaron tus ajustes", L"%ls %ls installiert - deine Einstellungen bleiben erhalten"}};
static const Str m_corrupt = {{L"The installer is corrupted. Please download it again.", L"O instalador está corrompido. Baixe de novo.",
                               L"El instalador está dañado. Descárgalo de nuevo.", L"Der Installer ist beschädigt. Bitte lade ihn erneut herunter."}};
static const Str m_copy_err = {{L"Could not write %ls (error %lu)", L"Não foi possível gravar %ls (erro %lu)", L"No se pudo escribir %ls (error %lu)", L"%ls konnte nicht geschrieben werden (Fehler %lu)"}};
static const Str m_verify_err = {{L"%ls does not match after copying", L"%ls não confere depois de copiado", L"%ls no coincide tras copiarlo", L"%ls stimmt nach dem Kopieren nicht überein"}};
static const Str m_running = {{L"%ls is running. Close the game and try again.", L"%ls está aberto. Feche o jogo e tente de novo.",
                               L"%ls está abierto. Cierra el juego e inténtalo de nuevo.", L"%ls läuft. Schließe das Spiel und versuche es erneut."}};
static const Str m_profile_ok = {{L"Profile \"%ls\": controls ready", L"Perfil \"%ls\": controles prontos", L"Perfil \"%ls\": controles listos", L"Profil \"%ls\": Steuerung bereit"}};
static const Str m_profile_same = {{L"Profile \"%ls\": already set", L"Perfil \"%ls\": já estava pronto", L"Perfil \"%ls\": ya estaba listo", L"Profil \"%ls\": bereits eingerichtet"}};
static const Str m_profile_restored = {{L"Profile \"%ls\": controls restored", L"Perfil \"%ls\": controles restaurados", L"Perfil \"%ls\": controles restaurados", L"Profil \"%ls\": Steuerung wiederhergestellt"}};
static const Str m_profile_bad = {{L"Profile \"%ls\": controls.sii not recognized, skipped", L"Perfil \"%ls\": controls.sii não reconhecido, ignorado",
                                   L"Perfil \"%ls\": controls.sii no reconocido, omitido", L"Profil \"%ls\": controls.sii nicht erkannt, übersprungen"}};
static const Str m_profile_err = {{L"Profile \"%ls\": could not save (error %lu)", L"Perfil \"%ls\": erro ao salvar (erro %lu)",
                                   L"Perfil \"%ls\": no se pudo guardar (error %lu)", L"Profil \"%ls\": konnte nicht gespeichert werden (Fehler %lu)"}};
static const Str m_profile_missing = {{L"Profile \"%ls\" has no controls yet: open the game with it, visit Options > Controls, quit and run this installer again.",
                                       L"O perfil \"%ls\" ainda não tem controles salvos: abra o jogo com ele, entre em Opções > Controles, saia e rode este instalador de novo.",
                                       L"El perfil \"%ls\" aún no tiene controles: abre el juego con él, entra en Opciones > Controles, sal y vuelve a ejecutar este instalador.",
                                       L"Profil \"%ls\" hat noch keine Steuerung: Starte das Spiel damit, öffne Optionen > Steuerung, beende es und starte diesen Installer erneut."}};
static const Str m_no_profiles = {{L"No profile found yet - create one in the game and run the installer again.",
                                   L"Nenhum perfil encontrado ainda - crie um no jogo e rode o instalador de novo.",
                                   L"Aún no hay perfiles - crea uno en el juego y vuelve a ejecutar el instalador.",
                                   L"Noch kein Profil gefunden - erstelle eins im Spiel und starte den Installer erneut."}};
static const Str m_no_slot = {{L"All controller slots are taken. Free one in Options > Controls and run again.",
                               L"Todos os espaços de controle estão ocupados. Libere um em Opções > Controles e rode de novo.",
                               L"Todas las ranuras de mando están ocupadas. Libera una en Opciones > Controles y vuelve a ejecutar.",
                               L"Alle Controller-Plätze sind belegt. Gib einen unter Optionen > Steuerung frei und starte erneut."}};
static const Str m_bad_format = {{L"This controls.sii has an unusual format; not everything could be connected.",
                                  L"Este controls.sii tem um formato diferente; não foi possível ligar tudo.",
                                  L"Este controls.sii tiene un formato distinto; no se pudo conectar todo.",
                                  L"Diese controls.sii hat ein ungewöhnliches Format; nicht alles konnte verbunden werden."}};
static const Str m_removed = {{L"%ls removed", L"%ls removido", L"%ls eliminado", L"%ls entfernt"}};
static const Str m_remove_err = {{L"Could not remove %ls (error %lu)", L"Erro ao remover %ls (erro %lu)", L"No se pudo eliminar %ls (error %lu)", L"%ls konnte nicht entfernt werden (Fehler %lu)"}};
static const Str m_no_games = {{L"No game selected.", L"Nenhum jogo selecionado.", L"Ningún juego seleccionado.", L"Kein Spiel ausgewählt."}};

}
