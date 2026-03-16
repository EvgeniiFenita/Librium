#!/usr/bin/env python3
"""Creates all test data used by Catch2 unit tests."""
import os
import sys
import zipfile

def main():
    if len(sys.argv) > 1:
        data_dir = sys.argv[1]
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        data_dir = os.path.join(script_dir, "data")
    
    archives_dir = os.path.join(data_dir, "archives")
    os.makedirs(archives_dir, exist_ok=True)

    # test.zip for ZipReader tests
    test_zip_path = os.path.join(data_dir, "test.zip")
    with zipfile.ZipFile(test_zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("hello.txt", "hello world from test zip")
        zf.writestr("subdir/another.txt", "another file")
    print(f"Created {test_zip_path}")

    # test_library.inpx for InpParser + Indexer tests
    SEP = "\x04"

    def inp_line(authors, genres, title, series="", serno="",
                 file_id="100001", size="1024000", deleted="0",
                 ext="fb2", date="2020-01-01", lang="ru",
                 rating="", keywords=""):
        return SEP.join([
            authors, genres, title, series, serno,
            file_id, size, file_id,
            deleted, ext, date, lang, rating, keywords, ""
        ])

    lines = [
        inp_line("Толстой,Лев,Николаевич:", "prose_classic:",
                 "Война и мир", series="Эпопея", serno="1",
                 file_id="100001", size="2048000", rating="5"),
        inp_line("Достоевский,Фёдор,Михайлович:", "prose_classic:",
                 "Преступление и наказание",
                 file_id="100002", size="1024000", rating="5"),
        inp_line("Удалённый,Автор,:", "unknown:",
                 "Удалённая книга", file_id="100003", size="512", deleted="1"),
        inp_line("King,Stephen,:", "thriller:",
                 "The Shining", file_id="100004", size="768000",
                 lang="en", rating="4"),
    ]

    inpx_path = os.path.join(data_dir, "test_library.inpx")
    with zipfile.ZipFile(inpx_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("fb2-test-001.zip.inp",
                    "\r\n".join(lines) + "\r\n")
        zf.writestr("collection.info", "Test Library\ntest_lib\n65536\nTest\n")
        zf.writestr("version.info", "20240101\r\n")

    print(f"Created {inpx_path}  (3 valid, 1 deleted)")

if __name__ == "__main__":
    main()
