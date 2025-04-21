# Eksportowanie plików z folderu 'src' do jednego pliku 'export.txt' w folderze 'output'
# z dodaniem nagłówków i znaczników końca pliku

import os

# Ścieżki do folderu źródłowego oraz pliku wynikowego
source_folder = "src"  # folder z plikami źródłowymi
output_path = os.path.join("export_files_to_txt", "export.txt")  # plik wynikowy

# Utwórz folder output, jeśli nie istnieje
os.makedirs(os.path.dirname(output_path), exist_ok=True)

with open(output_path, "wb") as export_file:  # otwieramy plik w trybie binarnym, aby bez zmian kopiować zawartość
    # Iteracja po plikach w folderze 'src' w uporządkowanej kolejności
    for filename in sorted(os.listdir(source_folder)):
        full_path = os.path.join(source_folder, filename)
        if os.path.isfile(full_path):
            # Dodaj nagłówek przed zawartością pliku
            header = f"### Eksport z pliku: {filename} ###\n\n"
            export_file.write(header.encode("utf-8"))
            
            # Odczytaj i zapisz całą zawartość pliku w trybie binarnym
            with open(full_path, "rb") as source_file:
                content = source_file.read()
                export_file.write(content)
                # Jeżeli plik nie kończy się newline, dodajemy nową linię
                if not content.endswith(b"\n"):
                    export_file.write(b"\n")
            
            # Dodaj marker końca zawartości pliku
            end_marker = f"\n### Koniec pliku: {filename} ###\n\n"
            export_file.write(end_marker.encode("utf-8"))

print(f"Pliki zostały wyeksportowane z markerami do {output_path}")
