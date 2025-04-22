import os
import sys
import io # Potrzebne do obsługi kodowania plików

# --- Konfiguracja ---

# Folder źródłowy WZGLĘDEM folderu workspace VS Code.
# '.' oznacza cały folder workspace. Możesz zmienić na np. 'src' lub 'scripts'.
FOLDER_ZRODLOWY_RELATYWNY = 'src'

# Nazwa folderu, w którym zostanie utworzony plik wynikowy (względem workspace)
FOLDER_EXPORTU_NAZWA = 'export_code'
# Nazwa pliku wynikowego
PLIK_EXPORTU_NAZWA = 'export.txt'

# Dodaj rozszerzenia (z kropką!), które chcesz pominąć w eksporcie.
# Rozszerzenia są sprawdzane bez względu na wielkość liter (np. .TMP i .tmp będą pominięte)
POMIJANE_ROZSZERZENIA = {".tmp", ".bak"}
# Przykład dodania kolejnych:
# POMIJANE_ROZSZERZENIA = {".tmp", ".bak", ".log", ".obj", ".pyc", ".git", ".vscode"}

# Dodatkowe foldery/pliki do pominięcia (ścieżki WZGLĘDEM workspace)
# Ważne, aby pominąć sam folder eksportu, by nie dołączał się do siebie!
POMIJANE_SCIEZKI = {FOLDER_EXPORTU_NAZWA}
# Przykład:
# POMIJANE_SCIEZKI = {FOLDER_EXPORTU_NAZWA, '.git', '.vscode', 'venv', '__pycache__'}

# --- Koniec Konfiguracji ---

def eksportuj_kod_do_pliku(workspace_root, src_relative, export_dir_name, export_filename, excluded_extensions, excluded_paths):
    """
    Przechodzi przez pliki w folderze źródłowym (względnym do workspace),
    łączy ich treść do jednego pliku tekstowego w folderze eksportu,
    pomijając podane rozszerzenia i ścieżki.
    """
    licznik_przetworzonych = 0
    licznik_pominietych_rozszerzenie = 0
    licznik_pominietych_sciezka = 0
    licznik_bledow_odczytu = 0

    abs_src_path = os.path.abspath(os.path.join(workspace_root, src_relative))
    abs_export_dir_path = os.path.join(workspace_root, export_dir_name)
    abs_export_file_path = os.path.join(abs_export_dir_path, export_filename)

    # Konwertuj rozszerzenia na małe litery dla spójnego porównania
    excluded_extensions_lower = {ext.lower() for ext in excluded_extensions}
    # Przygotuj pomijane ścieżki bezwzględne dla łatwiejszego porównania
    excluded_abs_paths = {os.path.abspath(os.path.join(workspace_root, p)) for p in excluded_paths}

    print(f"Folder źródłowy (absolutny): {abs_src_path}")
    print(f"Plik wynikowy (absolutny): {abs_export_file_path}")
    print(f"Pomijane rozszerzenia: {', '.join(excluded_extensions_lower)}")
    print(f"Pomijane ścieżki (względne): {', '.join(excluded_paths)}")

    # Sprawdź, czy folder źródłowy istnieje
    if not os.path.isdir(abs_src_path):
        print(f"BŁĄD: Folder źródłowy '{abs_src_path}' nie istnieje lub nie jest folderem.")
        return # Zakończ funkcję

    # Utwórz folder docelowy, jeśli nie istnieje
    try:
        os.makedirs(abs_export_dir_path, exist_ok=True)
    except OSError as e:
        print(f"BŁĄD: Nie można utworzyć folderu docelowego '{abs_export_dir_path}': {e}")
        return # Zakończ funkcję

    # Otwórz plik wynikowy w trybie zapisu ('w'), aby go nadpisać przy każdym uruchomieniu
    # Użyj kodowania UTF-8, które jest najczęstszym standardem
    try:
        with io.open(abs_export_file_path, 'w', encoding='utf-8') as outfile:
            outfile.write(f"# --- POCZĄTEK EKSPORTU KODU ---\n")
            outfile.write(f"# Źródło: {abs_src_path}\n")
            outfile.write(f"# Pominięte rozszerzenia: {', '.join(excluded_extensions_lower)}\n")
            outfile.write(f"# Pominięte ścieżki: {', '.join(excluded_paths)}\n")
            outfile.write(f"#\n")

            for root, dirs, files in os.walk(abs_src_path, topdown=True):
                # --- Pomijanie folderów ---
                # Sprawdź, czy bieżący folder (root) lub jego podfoldery są na liście pomijanych
                dirs_to_remove = []
                for d in dirs:
                    current_dir_abs_path = os.path.abspath(os.path.join(root, d))
                    # Sprawdź, czy ścieżka *zaczyna się* od jednej z pomijanych ścieżek bezwzględnych
                    # lub czy jest *dokładnie* tą ścieżką
                    is_excluded = False
                    for excluded_p in excluded_abs_paths:
                        if current_dir_abs_path == excluded_p or current_dir_abs_path.startswith(excluded_p + os.sep):
                            is_excluded = True
                            break
                    if is_excluded:
                        # print(f"Pomijanie folderu (i zawartości): {current_dir_abs_path}") # Debug
                        dirs_to_remove.append(d)
                        # Zlicz pliki w pomijanym folderze (szacunkowo, bo nie wchodzimy do środka)
                        # Można by zrobić bardziej precyzyjnie, ale to komplikuje kod
                        licznik_pominietych_sciezka += len(os.listdir(current_dir_abs_path)) # Liczba elementów w środku

                # Usuń zaznaczone foldery z listy dirs, aby os.walk do nich nie wchodził
                for d_to_remove in dirs_to_remove:
                    dirs.remove(d_to_remove)

                # --- Przetwarzanie plików ---
                for filename in files:
                    source_file_path = os.path.join(root, filename)
                    relative_file_path = os.path.relpath(source_file_path, workspace_root)

                    # Sprawdź, czy sam plik jest na liście pomijanych ścieżek
                    is_file_path_excluded = False
                    for excluded_p in excluded_abs_paths:
                         if source_file_path == excluded_p or source_file_path.startswith(excluded_p + os.sep):
                            is_file_path_excluded = True
                            break
                    if is_file_path_excluded:
                        # print(f"Pomijanie pliku (ścieżka): {relative_file_path}") # Debug
                        licznik_pominietych_sciezka += 1
                        continue

                    # Sprawdź rozszerzenie
                    _, extension = os.path.splitext(filename)
                    if extension.lower() in excluded_extensions_lower:
                        # print(f"Pomijanie pliku (rozszerzenie): {relative_file_path}") # Debug
                        licznik_pominietych_rozszerzenie += 1
                        continue

                    # Odczytaj zawartość pliku i dodaj do pliku wynikowego
                    try:
                        print(f"Przetwarzanie: {relative_file_path}")
                        outfile.write(f"\n{'=' * 40}\n")
                        outfile.write(f"=== Plik: {relative_file_path}\n")
                        outfile.write(f"{'=' * 40}\n\n")
                        # Odczytaj plik, próbując UTF-8, ale ignorując błędy kodowania
                        # errors='ignore' pominie znaki, których nie da się zdekodować
                        # Alternatywnie można spróbować innych kodowań np. 'cp1250' dla Windows
                        with io.open(source_file_path, 'r', encoding='utf-8', errors='ignore') as infile:
                            outfile.write(infile.read())
                        outfile.write("\n") # Dodaj nową linię dla pewności
                        licznik_przetworzonych += 1
                    except Exception as e:
                        print(f"BŁĄD podczas odczytu pliku {source_file_path}: {e}")
                        outfile.write(f"\n--- BŁĄD ODCZYTU PLIKU: {relative_file_path} ({e}) ---\n")
                        licznik_bledow_odczytu += 1

            outfile.write(f"\n# --- KONIEC EKSPORTU KODU ---\n")

    except IOError as e:
        print(f"BŁĄD: Nie można zapisać do pliku wynikowego '{abs_export_file_path}': {e}")
        return

    print("\n--- Podsumowanie ---")
    print(f"🔍 Przetworzono i dodano do pliku: {licznik_przetworzonych}")
    print(f"⏩ Pominięto plików (rozszerzenie): {licznik_pominietych_rozszerzenie}")
    print(f"⏩ Pominięto plików/folderów (ścieżka): {licznik_pominietych_sciezka} (szacunkowo)")
    print(f"❌ Wystąpiło błędów odczytu plików: {licznik_bledow_odczytu}")
    print(f"✅ Wynik zapisano w: {abs_export_file_path}")


# --- Główna część skryptu ---
if __name__ == "__main__":
    print("--- Rozpoczęcie eksportu kodu do pliku ---")

    # Pobierz ścieżkę do bieżącego folderu roboczego - zakładamy, że to workspace VS Code
    workspace_sciezka = os.getcwd()
    print(f"Wykryty folder Workspace: {workspace_sciezka}")

    # Wywołaj funkcję eksportującą
    eksportuj_kod_do_pliku(
        workspace_root=workspace_sciezka,
        src_relative=FOLDER_ZRODLOWY_RELATYWNY,
        export_dir_name=FOLDER_EXPORTU_NAZWA,
        export_filename=PLIK_EXPORTU_NAZWA,
        excluded_extensions=POMIJANE_ROZSZERZENIA,
        excluded_paths=POMIJANE_SCIEZKI
    )

    print("\n--- Eksport zakończony ---")