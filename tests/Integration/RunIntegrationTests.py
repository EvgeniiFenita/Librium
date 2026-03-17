import os
import sys
import json
import shutil
import zipfile
import subprocess
import argparse
from pathlib import Path

# --- Constants & Helpers ---
SEP = "\x04"

def make_inp_line(authors, genres, title, series="", serno="",
                 file_id="100001", size="102400", deleted="0",
                 ext="fb2", date="2020-01-01", lang="ru",
                 rating="", keywords=""):
    return SEP.join([
        authors, genres, title, series, serno,
        file_id, size, file_id,
        deleted, ext, date, lang, rating, keywords, ""
    ])

def make_minimal_fb2(title, author_last_name, annotation="", publisher="", year=""):
    ann_xml = f"<annotation><p>{annotation}</p></annotation>" if annotation else ""
    pub_xml = ""
    if publisher or year:
        pub_xml = "<publish-info>"
        if publisher: pub_xml += f"<publisher>{publisher}</publisher>"
        if year:      pub_xml += f"<year>{year}</year>"
        pub_xml += "</publish-info>"
    
    return f"""<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description>
    <title-info>
      <author><last-name>{author_last_name}</last-name></author>
      <book-title>{title}</book-title>
      {ann_xml}
    </title-info>
    {pub_xml}
  </description>
  <body><section><p>.</p></section></body>
</FictionBook>
"""

class TestLibraryGenerator:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.v1_dir = self.root_dir / "v1"
        self.v2_dir = self.root_dir / "v2"

    def generate_v1(self):
        arch_dir = self.v1_dir / "archives"
        arch_dir.mkdir(parents=True, exist_ok=True)

        # fb2-v1-001.zip
        a1 = {
            "200001.fb2": make_minimal_fb2("Война и мир", "Толстой", "Великий роман о войне и мире.", "Худ. лит.", "1978"),
            "200002.fb2": make_minimal_fb2("Преступление и наказание", "Достоевский"),
            "200004.fb2": make_minimal_fb2("The Shining", "King", "A terrifying tale."),
            "200005.fb2": make_minimal_fb2("Пикник на обочине", "Стругацкий", "Классика советской фантастики.")
        }
        self._create_zip(arch_dir / "fb2-v1-001.zip", a1)

        # fb2-v1-002.zip
        a2 = {
            "200006.fb2": make_minimal_fb2("Murder on the Orient Express", "Christie"),
            "200007.fb2": make_minimal_fb2("Мастер и Маргарита", "Булгаков", "Роман о дьяволе, посетившем Москву."),
            "200008.fb2": make_minimal_fb2("Неизвестная книга", "Неизвестен")
        }
        self._create_zip(arch_dir / "fb2-v1-002.zip", a2)

        # library.inpx
        v1_inp1 = "\r\n".join([
            make_inp_line("Толстой,Лев,Николаевич:", "prose_classic:", "Война и мир", "Эпопея", "1", "200001", "2048000", date="2020-01-15", lang="ru", rating="5"),
            make_inp_line("Достоевский,Фёдор,Михайлович:", "prose_classic:", "Преступление и наказание", file_id="200002", size="1024000", date="2020-02-10", lang="ru", rating="5"),
            make_inp_line("Удалённый,Автор,:", "unknown:", "Удалённая книга", file_id="200003", size="512", deleted="1", date="2020-03-01", lang="ru"),
            make_inp_line("King,Stephen,:", "thriller:", "The Shining", file_id="200004", size="768000", date="2020-04-01", lang="en", rating="4"),
            make_inp_line("Стругацкий,Аркадий,Натанович:Стругацкий,Борис,Натанович:", "sf:", "Пикник на обочине", "Мир Полудня", "3", "200005", "512000", date="2021-06-01", lang="ru", rating="5")
        ]) + "\r\n"

        v1_inp2 = "\r\n".join([
            make_inp_line("Christie,Agatha,:", "detective:", "Murder on the Orient Express", file_id="200006", size="480000", date="2021-01-20", lang="en", rating="4"),
            make_inp_line("Булгаков,Михаил,Афанасьевич:", "prose_classic:", "Мастер и Маргарита", file_id="200007", size="900000", date="2021-03-05", lang="ru", rating="5"),
            make_inp_line("Неизвестен,,:", "sf:", "Неизвестная книга", file_id="200008", size="256000", date="2022-07-14", lang="ru", rating="3")
        ]) + "\r\n"

        self._create_inpx(self.v1_dir / "library.inpx", {
            "fb2-v1-001.zip.inp": v1_inp1,
            "fb2-v1-002.zip.inp": v1_inp2
        })

    def generate_v2(self):
        arch_v1 = self.v1_dir / "archives"
        arch_v2 = self.v2_dir / "archives"
        arch_v2.mkdir(parents=True, exist_ok=True)

        shutil.copy(arch_v1 / "fb2-v1-001.zip", arch_v2)
        shutil.copy(arch_v1 / "fb2-v1-002.zip", arch_v2)

        # fb2-v2-001.zip
        a3 = {
            "200009.fb2": make_minimal_fb2("Вишнёвый сад", "Чехов", "Пьеса о гибели дворянского уклада."),
            "200010.fb2": make_minimal_fb2("Foundation", "Asimov"),
            "200011.fb2": make_minimal_fb2("Анна Каренина", "Толстой")
        }
        self._create_zip(arch_v2 / "fb2-v2-001.zip", a3)

        v2_inp3 = "\r\n".join([
            make_inp_line("Чехов,Антон,Павлович:", "prose_classic:", "Вишнёвый сад", file_id="200009", size="310000", date="2023-05-01", lang="ru", rating="4"),
            make_inp_line("Asimov,Isaac,:", "sf:", "Foundation", "Foundation", "1", "200010", "620000", date="2023-06-15", lang="en", rating="5"),
            make_inp_line("Толстой,Лев,Николаевич:", "prose_classic:", "Анна Каренина", file_id="200011", size="1800000", date="2023-08-20", lang="ru", rating="5")
        ]) + "\r\n"

        # Re-use v1 INP content
        v1_inp1 = zipfile.ZipFile(self.v1_dir / "library.inpx").read("fb2-v1-001.zip.inp").decode("utf-8")
        v1_inp2 = zipfile.ZipFile(self.v1_dir / "library.inpx").read("fb2-v1-002.zip.inp").decode("utf-8")

        self._create_inpx(self.v2_dir / "library.inpx", {
            "fb2-v1-001.zip.inp": v1_inp1,
            "fb2-v1-002.zip.inp": v1_inp2,
            "fb2-v2-001.zip.inp": v2_inp3
        })

    def _create_zip(self, path, entries):
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
            for name, content in entries.items():
                z.writestr(name, content)

    def _create_inpx(self, path, entries):
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
            for name, content in entries.items():
                z.writestr(name, content)
            z.writestr('collection.info', 'Integration Test Library\ntest\n65536\nTest\n')
            z.writestr('version.info', '20240101\r\n')

class IntegrationTester:
    def __init__(self, librium_exe, data_dir):
        self.librium_exe = librium_exe
        self.data_dir = Path(data_dir)
        self.db_path = self.data_dir / "library.db"
        self.output_dir = self.data_dir / "out"
        self.pass_count = 0
        self.fail_count = 0

    def run(self):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        try:
            self.test_full_import()
            self.test_upgrade_import()
            self.test_queries()
        except Exception as e:
            print(f"\n[FATAL ERROR] {e}")
            import traceback
            traceback.print_exc()
            return False

        print(f"\nIntegration Tests Summary:")
        print(f"  Passed: {self.pass_count}")
        print(f"  Failed: {self.fail_count}")
        return self.fail_count == 0

    def assert_equal(self, actual, expected, description):
        if actual == expected:
            print(f"  [PASS] {description}")
            self.pass_count += 1
        else:
            print(f"  [FAIL] {description}")
            print(f"         Expected: {expected}")
            print(f"         Actual:   {actual}")
            self.fail_count += 1

    def run_librium(self, args):
        cmd = [self.librium_exe] + args
        res = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8")
        if res.returncode != 0:
            print(f"Command failed: {' '.join(cmd)}")
            print(f"Exit code: {res.returncode}")
            print(f"Stderr: {res.stderr}")
        return res

    def run_query(self, out_name, extra_args=None):
        out_path = self.output_dir / out_name
        args = ["query", "--db", str(self.db_path), "--output", str(out_path)]
        if extra_args:
            args.extend(extra_args)
        
        res = self.run_librium(args)
        if res.returncode != 0:
            return None
        
        with open(out_path, "r", encoding="utf-8") as f:
            return json.load(f)

    def create_config(self, lib_path, name):
        cfg = {
            "library": {
                "inpxPath": str(lib_path / "library.inpx"),
                "archivesDir": str(lib_path / "archives")
            },
            "database": {
                "path": str(self.db_path)
            },
            "import": {
                "threadCount": 4,
                "parseFb2": True
            },
            "logging": {
                "level": "info",
                "file": str(self.data_dir / "integration.log")
            }
        }
        cfg_path = self.data_dir / f"{name}_config.json"
        with open(cfg_path, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)
        return cfg_path

    def test_full_import(self):
        print("\n--- Test: Full Import (V1) ---")
        cfg_v1 = self.create_config(self.data_dir / "v1", "v1")
        
        if self.db_path.exists(): self.db_path.unlink()
        
        res = self.run_librium(["import", "--config", str(cfg_v1)])
        self.assert_equal(res.returncode, 0, "Librium import exits with 0")
        
        data = self.run_query("v1_all.json", ["--limit", "0"])
        self.assert_equal(data["totalFound"], 7, "Found 7 books in V1")

    def test_upgrade_import(self):
        print("\n--- Test: Upgrade Import (V2) ---")
        cfg_v2 = self.create_config(self.data_dir / "v2", "v2")
        
        res = self.run_librium(["upgrade", "--config", str(cfg_v2)])
        self.assert_equal(res.returncode, 0, "Librium upgrade exits with 0")
        
        data = self.run_query("v2_all.json", ["--limit", "0"])
        self.assert_equal(data["totalFound"], 10, "Found 10 books in V2")

    def test_queries(self):
        print("\n--- Test: Queries ---")
        
        # Language
        q_ru = self.run_query("q_ru.json", ["--language", "ru", "--limit", "0"])
        self.assert_equal(q_ru["totalFound"], 7, "ru: 7 books")
        
        q_en = self.run_query("q_en.json", ["--language", "en", "--limit", "0"])
        self.assert_equal(q_en["totalFound"], 3, "en: 3 books")

        # Author (Cyrillic)
        q_tolstoy = self.run_query("q_tolstoy.json", ["--author", "Толстой", "--limit", "0"])
        self.assert_equal(q_tolstoy["totalFound"], 2, "Author=Толстой: 2 books")
        
        # Genre
        q_classic = self.run_query("q_classic.json", ["--genre", "prose_classic", "--limit", "0"])
        self.assert_equal(q_classic["totalFound"], 5, "Genre=prose_classic: 5 books")

        # Annotation
        q_ann = self.run_query("q_ann.json", ["--with-annotation", "--limit", "0"])
        self.assert_equal(q_ann["totalFound"], 5, "--with-annotation: 5 books")

        # Rating
        q_r5 = self.run_query("q_rating5.json", ["--rating-min", "5", "--limit", "0"])
        self.assert_equal(q_r5["totalFound"], 6, "Rating>=5: 6 books")

        # Date range
        q_date = self.run_query("q_date.json", ["--date-from", "2023-01-01", "--limit", "0"])
        self.assert_equal(q_date["totalFound"], 3, "Added since 2023: 3 books")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--librium", required=True)
    parser.add_argument("--data-dir", required=True)
    parser.add_argument("--keep-data", action="store_true")
    args = parser.parse_args()

    data_dir = Path(args.data_dir)
    if data_dir.exists() and not args.keep_data:
        shutil.rmtree(data_dir)
    data_dir.mkdir(parents=True, exist_ok=True)

    print(f"Generating test library in {data_dir}...")
    gen = TestLibraryGenerator(data_dir)
    gen.generate_v1()
    gen.generate_v2()

    tester = IntegrationTester(args.librium, data_dir)
    success = tester.run()
    
    if not args.keep_data:
        shutil.rmtree(data_dir)
    
    sys.exit(0 if success else 1)
