#pragma once

enum Lang { LANG_EN, LANG_PT, LANG_ES, LANG_DE, LANG_COUNT };

struct Str { const wchar_t *t[LANG_COUNT]; };

extern Lang g_lang;
inline const wchar_t *tr(const Str &s) { return s.t[g_lang]; }

namespace txt {

static const Str lang_name[LANG_COUNT] = {
    {{L"English", L"English", L"English", L"English"}},
    {{L"Português", L"Português", L"Português", L"Português"}},
    {{L"Español", L"Español", L"Español", L"Español"}},
    {{L"Deutsch", L"Deutsch", L"Deutsch", L"Deutsch"}},
};
static const char *const lang_code[LANG_COUNT] = {"en", "pt", "es", "de"};

static const Str installer = {{L"Installer", L"Instalador", L"Instalador", L"Installer"}};
static const Str choose_lang = {{L"Choose your language", L"Escolha o idioma", L"Elige el idioma", L"Sprache wählen"}};

static const Str step_lang = {{L"Language", L"Idioma", L"Idioma", L"Sprache"}};
static const Str step_mods = {{L"Mods", L"Mods", L"Mods", L"Mods"}};
static const Str step_games = {{L"Games", L"Jogos", L"Juegos", L"Spiele"}};
static const Str step_install = {{L"Install", L"Instalar", L"Instalar", L"Installieren"}};

static const Str mods_title = {{L"Two plugins in a single installer", L"São dois plugins num instalador só",
                                L"Dos plugins en un solo instalador", L"Zwei Plugins in einem Installer"}};
static const Str mods_sub = {{L"You can run both together, which is how they were designed, or untick either one.",
                              L"Dá para usar os dois juntos, que é como eles foram pensados, ou desmarcar um deles.",
                              L"Puedes usar los dos juntos, que es como se pensaron, o desmarcar uno.",
                              L"Du kannst beide zusammen nutzen, so sind sie gedacht, oder eins abwählen."}};

static const Str drive_tag = {{L"Steering, throttle and brakes on the keyboard", L"Volante, acelerador e freio no teclado",
                               L"Volante, acelerador y freno en el teclado", L"Lenkung, Gas und Bremse auf der Tastatur"}};
static const Str view_tag = {{L"A cab camera that moves like a head", L"A câmera da cabine se mexendo como uma cabeça",
                              L"La cámara de la cabina moviéndose como una cabeza", L"Eine Kabinenkamera, die sich wie ein Kopf bewegt"}};

static const Str drive_f[4] = {
    {{L"Weighted steering", L"Volante com peso", L"Volante con peso", L"Lenkung mit Gewicht"}},
    {{L"Progressive throttle and brakes", L"Acelerador e freio progressivos", L"Acelerador y freno progresivos", L"Progressives Gas und Bremse"}},
    {{L"Cargo weight", L"Peso da carga", L"Peso de la carga", L"Ladungsgewicht"}},
    {{L"Surface grip", L"Aderência do piso", L"Agarre del suelo", L"Grip des Untergrunds"}},
};
static const Str view_f[4] = {
    {{L"Look with weight and inertia", L"Olhar com peso e inércia", L"Mirar con peso e inercia", L"Blick mit Gewicht und Trägheit"}},
    {{L"Head zoom", L"Zoom de cabeça", L"Zoom de cabeza", L"Kopf-Zoom"}},
    {{L"Steadier horizon", L"Horizonte estabilizado", L"Horizonte estable", L"Ruhiger Horizont"}},
    {{L"Body inertia", L"Inércia do corpo", L"Inercia del cuerpo", L"Körperträgheit"}},
};

static const Str also = {{L"There's also", L"Tem também o", L"También está", L"Es gibt auch"}};
static const Str games_title = {{L"Where to install", L"Onde instalar", L"Dónde instalar", L"Wo installieren"}};
static const Str games_sub = {{L"Close the game before you continue.", L"Feche o jogo antes de continuar.",
                               L"Cierra el juego antes de seguir.", L"Schließ das Spiel, bevor du weitermachst."}};
static const Str no_games = {{L"Couldn't find the game. Point to its folder below.", L"Não achamos o jogo. Indique a pasta dele abaixo.",
                              L"No encontramos el juego. Indica su carpeta abajo.", L"Spiel nicht gefunden. Gib unten den Ordner an."}};
static const Str add_folder = {{L"Add game folder", L"Adicionar pasta do jogo", L"Añadir carpeta del juego", L"Spielordner hinzufügen"}};
static const Str folder_title = {{L"Game folder (the one with \"bin\" inside)", L"Pasta do jogo (a que tem a pasta \"bin\")",
                                  L"Carpeta del juego (la que tiene \"bin\")", L"Spielordner (der mit \"bin\")"}};
static const Str folder_bad = {{L"That folder doesn't have the game (bin\\win_x64\\eurotrucks2.exe or amtrucks.exe).",
                                L"Essa pasta não tem o jogo (bin\\win_x64\\eurotrucks2.exe ou amtrucks.exe).",
                                L"Esa carpeta no tiene el juego (bin\\win_x64\\eurotrucks2.exe o amtrucks.exe).",
                                L"In diesem Ordner ist kein Spiel (bin\\win_x64\\eurotrucks2.exe oder amtrucks.exe)."}};
static const Str st_not_installed = {{L"Not installed", L"Não instalado", L"No instalado", L"Nicht installiert"}};
static const Str st_installed = {{L"Installed", L"Instalado", L"Instalado", L"Installiert"}};
static const Str st_old = {{L"Old version", L"Versão antiga", L"Versión antigua", L"Alte Version"}};
static const Str st_running = {{L"Game running", L"Jogo aberto", L"Juego abierto", L"Spiel läuft"}};

static const Str btn_next = {{L"Next", L"Continuar", L"Siguiente", L"Weiter"}};
static const Str btn_back = {{L"Back", L"Voltar", L"Atrás", L"Zurück"}};
static const Str btn_install = {{L"Install", L"Instalar", L"Instalar", L"Installieren"}};
static const Str btn_update = {{L"Update", L"Atualizar", L"Actualizar", L"Aktualisieren"}};
static const Str btn_uninstall = {{L"Uninstall", L"Desinstalar", L"Desinstalar", L"Deinstallieren"}};
static const Str btn_finish = {{L"Done", L"Concluir", L"Listo", L"Fertig"}};
static const Str btn_launch = {{L"Open the game", L"Abrir o jogo", L"Abrir el juego", L"Spiel öffnen"}};
static const Str btn_download = {{L"Download", L"Baixar", L"Descargar", L"Herunterladen"}};
static const Str btn_retry_admin = {{L"Try as administrator", L"Tentar como administrador", L"Probar como administrador", L"Als Administrator versuchen"}};

static const Str installing = {{L"Installing", L"Instalando", L"Instalando", L"Installiere"}};
static const Str removing = {{L"Removing", L"Removendo", L"Eliminando", L"Entferne"}};
static const Str done_title = {{L"Done.", L"Pronto.", L"Listo.", L"Fertig."}};
static const Str done_sub = {{L"Start the game and click OK. You'll see a notice about advanced SDK features, which is normal for any plugin.",
                              L"Abra o jogo e clique em OK. Vai aparecer um aviso sobre recursos avançados do SDK, que é normal para qualquer plugin.",
                              L"Abre el juego y haz clic en Aceptar. Verás un aviso sobre funciones avanzadas del SDK, que es normal en cualquier plugin.",
                              L"Starte das Spiel und klick auf OK. Du siehst einen Hinweis zu erweiterten SDK-Funktionen, das ist bei jedem Plugin normal."}};
static const Str done_warn_title = {{L"Finished with warnings", L"Terminou com avisos", L"Terminó con avisos", L"Mit Hinweisen fertig"}};
static const Str removed_title = {{L"Uninstalled.", L"Desinstalado.", L"Desinstalado.", L"Deinstalliert."}};
static const Str removed_sub = {{L"Your controls are back to normal. Your settings are saved in case you come back.",
                                 L"Os controles voltaram ao normal. Seus ajustes ficaram salvos, caso volte.",
                                 L"Los controles volvieron a la normalidad. Tus ajustes quedan guardados por si vuelves.",
                                 L"Die Steuerung ist wieder wie vorher. Deine Einstellungen bleiben gespeichert."}};
static const Str admin_needed = {{L"Windows blocked the game folder", L"O Windows bloqueou a pasta do jogo",
                                  L"Windows bloqueó la carpeta del juego", L"Windows hat den Spielordner gesperrt"}};

static const Str controls_title = {{L"Controls", L"Comandos", L"Controles", L"Steuerung"}};
static const Str controls_head = {{L"Nothing new to learn", L"Você não precisa reaprender nada", L"No hay nada nuevo que aprender", L"Nichts Neues zu lernen"}};
static const Str ctl_steer = {{L"Steering", L"Volante", L"Volante", L"Lenkung"}};
static const Str ctl_steer_d = {{L"tap to correct, hold to turn", L"toque corrige, segurar faz a curva",
                                 L"un toque corrige, mantener hace la curva", L"tippen korrigiert, halten lenkt ein"}};
static const Str ctl_thr = {{L"Throttle", L"Acelerador", L"Acelerador", L"Gas"}};
static const Str ctl_thr_d = {{L"progressive, double tap to floor it", L"progressivo, e dois toques afundam o pé",
                               L"progresivo, doble toque para pisar a fondo", L"progressiv, doppelt tippen für Vollgas"}};
static const Str ctl_brk = {{L"Brake", L"Freio", L"Freno", L"Bremse"}};
static const Str ctl_brk_d = {{L"progressive, double tap to brake hard", L"progressivo, e dois toques freiam forte",
                               L"progresivo, doble toque para frenar fuerte", L"progressiv, doppelt tippen für Vollbremsung"}};
static const Str ctl_look = {{L"Look", L"Olhar", L"Mirar", L"Umsehen"}};
static const Str ctl_look_d = {{L"with weight and inertia, interior camera only", L"com peso e inércia, só na câmera interna",
                                L"con peso e inercia, solo en la cámara interior", L"mit Gewicht und Trägheit, nur in der Innenkamera"}};
static const Str ctl_middle = {{L"Middle button", L"Botão do meio", L"Botón central", L"Mittlere Taste"}};
static const Str ctl_zoom = {{L"Head zoom", L"Zoom de cabeça", L"Zoom de cabeza", L"Kopf-Zoom"}};
static const Str ctl_zoom_d = {{L"your body leans where you look", L"o corpo vai na direção do olhar", L"el cuerpo se inclina hacia donde miras", L"der Körper beugt sich in Blickrichtung"}};
static const Str ctl_ini = {{L"Fine-tune everything in sznt-drive.ini and sznt-view.ini, in the game's plugins folder.",
                             L"Ajustes finos em sznt-drive.ini e sznt-view.ini, na pasta plugins do jogo.",
                             L"Ajustes finos en sznt-drive.ini y sznt-view.ini, en la carpeta plugins del juego.",
                             L"Feineinstellungen in sznt-drive.ini und sznt-view.ini im plugins-Ordner des Spiels."}};

static const Str uninstall_title = {{L"Uninstall", L"Desinstalar", L"Desinstalar", L"Deinstallieren"}};
static const Str uninstall_sub = {{L"Deletes the plugin files and puts your controls back as they were.",
                                   L"Apaga os arquivos do plugin e devolve os controles como estavam.",
                                   L"Borra los archivos del plugin y deja los controles como estaban.",
                                   L"Löscht die Plugin-Dateien und stellt die Steuerung wieder her."}};

static const Str update_available = {{L"Version %ls available", L"Versão %ls disponível", L"Versión %ls disponible", L"Version %ls verfügbar"}};

// installer messages
static const Str m_old_disabled = {{L"Old version turned off (%ls)", L"Versão antiga desativada (%ls)", L"Versión antigua desactivada (%ls)", L"Alte Version deaktiviert (%ls)"}};
static const Str m_installed = {{L"%ls %ls installed", L"%ls %ls instalado", L"%ls %ls instalado", L"%ls %ls installiert"}};
static const Str m_kept = {{L"%ls %ls installed, your settings were kept", L"%ls %ls instalado, seus ajustes foram mantidos",
                            L"%ls %ls instalado, tus ajustes se mantuvieron", L"%ls %ls installiert, deine Einstellungen bleiben"}};
static const Str m_corrupt = {{L"The installer is damaged. Download it again.", L"O instalador está corrompido. Baixe de novo.",
                               L"El instalador está dañado. Descárgalo de nuevo.", L"Der Installer ist beschädigt. Lade ihn neu herunter."}};
static const Str m_copy_err = {{L"Couldn't write %ls (error %lu)", L"Não deu para gravar %ls (erro %lu)", L"No se pudo escribir %ls (error %lu)", L"%ls konnte nicht geschrieben werden (Fehler %lu)"}};
static const Str m_verify_err = {{L"%ls doesn't match after copying", L"%ls não confere depois de copiado", L"%ls no coincide tras copiarlo", L"%ls stimmt nach dem Kopieren nicht"}};
static const Str m_running = {{L"%ls is running. Close it and try again.", L"%ls está aberto. Feche e tente de novo.",
                               L"%ls está abierto. Ciérralo e inténtalo de nuevo.", L"%ls läuft. Schließ es und versuch es nochmal."}};
static const Str m_profile_ok = {{L"Profile \"%ls\": controls ready", L"Perfil \"%ls\": controles prontos", L"Perfil \"%ls\": controles listos", L"Profil \"%ls\": Steuerung bereit"}};
static const Str m_profile_same = {{L"Profile \"%ls\": already set", L"Perfil \"%ls\": já estava pronto", L"Perfil \"%ls\": ya estaba listo", L"Profil \"%ls\": schon eingerichtet"}};
static const Str m_profile_restored = {{L"Profile \"%ls\": controls restored", L"Perfil \"%ls\": controles restaurados", L"Perfil \"%ls\": controles restaurados", L"Profil \"%ls\": Steuerung wiederhergestellt"}};
static const Str m_profile_bad = {{L"Profile \"%ls\": unknown controls file, skipped", L"Perfil \"%ls\": arquivo de controles diferente, pulado",
                                   L"Perfil \"%ls\": archivo de controles desconocido, omitido", L"Profil \"%ls\": unbekannte Steuerungsdatei, übersprungen"}};
static const Str m_profile_err = {{L"Profile \"%ls\": couldn't save (error %lu)", L"Perfil \"%ls\": não deu para salvar (erro %lu)",
                                   L"Perfil \"%ls\": no se pudo guardar (error %lu)", L"Profil \"%ls\": Speichern fehlgeschlagen (Fehler %lu)"}};
static const Str m_profile_missing = {{L"Profile \"%ls\" has no saved controls yet. Open the game with it, go to Options > Controls, quit and run the installer again.",
                                       L"O perfil \"%ls\" ainda não tem controles salvos. Abra o jogo com ele, entre em Opções > Controles, saia e rode o instalador de novo.",
                                       L"El perfil \"%ls\" aún no tiene controles guardados. Abre el juego con él, entra en Opciones > Controles, sal y vuelve a ejecutar el instalador.",
                                       L"Profil \"%ls\" hat noch keine gespeicherte Steuerung. Starte das Spiel damit, öffne Optionen > Steuerung, beende es und starte den Installer neu."}};
static const Str m_no_profiles = {{L"No profile yet. Create one in the game and run the installer again.",
                                   L"Nenhum perfil ainda. Crie um no jogo e rode o instalador de novo.",
                                   L"Aún no hay perfiles. Crea uno en el juego y vuelve a ejecutar el instalador.",
                                   L"Noch kein Profil. Leg eins im Spiel an und starte den Installer neu."}};
static const Str m_no_slot = {{L"All controller slots are taken. Free one in Options > Controls.",
                               L"Todos os espaços de controle estão ocupados. Libere um em Opções > Controles.",
                               L"Todas las ranuras de mando están ocupadas. Libera una en Opciones > Controles.",
                               L"Alle Controller-Plätze sind belegt. Gib einen unter Optionen > Steuerung frei."}};
static const Str m_bad_format = {{L"This controls file is different from the usual; some bindings were skipped.",
                                  L"Este arquivo de controles é diferente do normal; algumas ligações ficaram de fora.",
                                  L"Este archivo de controles es distinto; algunas asignaciones se omitieron.",
                                  L"Diese Steuerungsdatei ist ungewöhnlich; einige Zuweisungen fehlen."}};
static const Str m_removed = {{L"%ls removed", L"%ls removido", L"%ls eliminado", L"%ls entfernt"}};
static const Str m_remove_err = {{L"Couldn't remove %ls (error %lu)", L"Não deu para remover %ls (erro %lu)", L"No se pudo eliminar %ls (error %lu)", L"%ls konnte nicht entfernt werden (Fehler %lu)"}};
static const Str m_no_games = {{L"No game selected.", L"Nenhum jogo selecionado.", L"Ningún juego seleccionado.", L"Kein Spiel ausgewählt."}};

}
