#!/usr/bin/env python3
import os
import sys

# Prevent creation of __pycache__ folders
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

import zipfile
import uuid
import random
import argparse
from datetime import datetime
from pathlib import Path

# Add the scripts directory to path to import Core
sys.path.append(str(Path(__file__).parent))
from Core import CUI, CPaths

class CLibraryGenerator:
    """Generator of realistic test data for Librium."""
    
    SEP = "\x04"
    
    def __init__(self, output_dir):
        self.output_dir = Path(output_dir)
        self.lib_path = self.output_dir / "test_library"
        self.lib_path.mkdir(parents=True, exist_ok=True)
        self.books = []
        self.archives = {}
        self.next_id = 100001

    def add_book(self, title, authors, genres=None, lang="ru", deleted=False, 
                 series="", ser_no="", archive="fb2-001.zip", lib_id=None,
                 annotation=None, keywords="", size=None, date=None, rating=0,
                 raw_fb2=None, has_cover=False):
        """Adds a book description to the generation queue."""
        if not lib_id:
            lib_id = str(self.next_id)
            self.next_id += 1
        
        book = {
            "title": title,
            "authors": authors if isinstance(authors, list) else [authors],
            "genres": genres if isinstance(genres, list) else ([genres] if genres else ["prose_classic"]),
            "lang": lang,
            "deleted": "1" if deleted else "0",
            "series": series,
            "ser_no": str(ser_no),
            "file": lib_id,
            "size": str(size if size is not None else random.randint(10000, 1000000)),
            "lib_id": lib_id,
            "archive": archive,
            "date": date if date else datetime.now().strftime("%Y-%m-%d"),
            "rating": str(rating),
            "annotation": annotation,
            "keywords": keywords,
            "raw_fb2": raw_fb2,
            "has_cover": has_cover
        }
        self.books.append(book)
        
        if archive not in self.archives:
            self.archives[archive] = []
        self.archives[archive].append(book)
        return lib_id

    def _format_authors(self, authors):
        # Format: Last,First,Middle:Last2,First2,Middle2:
        formatted = []
        for a in authors:
            parts = [p.strip() for p in a.split(",")]
            while len(parts) < 3:
                parts.append("")
            formatted.append(",".join(parts[:3]))
        return ":".join(formatted) + ":"

    def _format_genres(self, genres):
        return ":".join(genres) + ":"

    def generate(self):
        """Physically creates library files on disk."""
        # 1. Create ZIP archives with FB2
        for arch_name, books in self.archives.items():
            arch_path = self.lib_path / arch_name
            with zipfile.ZipFile(arch_path, "w", zipfile.ZIP_DEFLATED) as zf:
                for b in books:
                    fb2_content = b['raw_fb2'] if b['raw_fb2'] else self._generate_fb2(b)
                    zf.writestr(f"{b['file']}.fb2", fb2_content)

        # 2. Create INPX
        inpx_path = self.lib_path / "library.inpx"
        with zipfile.ZipFile(inpx_path, "w", zipfile.ZIP_DEFLATED) as zf:
            # Group records by archives for .inp files
            for arch_name, books in self.archives.items():
                inp_name = arch_name.replace(".zip", ".inp")
                # In INPX, the archive name is usually written WITHOUT the .zip extension
                arch_name_in_inpx = arch_name.replace(".zip", "")

                inp_lines = []
                for b in books:
                    line = self.SEP.join([
                        self._format_authors(b['authors']),
                        self._format_genres(b['genres']),
                        b['title'], b['series'], b['ser_no'],
                        b['file'], b['size'], b['lib_id'],
                        b['deleted'], "fb2", b['date'], b['lang'], 
                        b['rating'], b['keywords'], arch_name_in_inpx, ""
                    ])
                    inp_lines.append(line)
                
                zf.writestr(inp_name, "\r\n".join(inp_lines) + "\r\n")
            
            zf.writestr("collection.info", "Test Library\ntest_lib\n65536\n")
            zf.writestr("version.info", datetime.now().strftime("%Y%m%d") + "\n")
        
        return str(self.lib_path)

    def _generate_fb2(self, book):
        annotation_xml = f"<annotation><p>{book['annotation']}</p></annotation>" if book['annotation'] else ""
        keywords_xml = f"<keywords>{book['keywords']}</keywords>" if book['keywords'] else ""
        
        author_xml = ""
        for a_str in book['authors']:
            parts = [p.strip() for p in a_str.split(",")]
            last = parts[0] if len(parts) > 0 else ""
            first = parts[1] if len(parts) > 1 else ""
            middle = parts[2] if len(parts) > 2 else ""
            
            author_xml += f"<author><first-name>{first}</first-name><last-name>{last}</last-name><middle-name>{middle}</middle-name></author>"

        cover_xml = ""
        binary_xml = ""
        if book.get('has_cover'):
            cover_xml = '<coverpage><image l:href="#cover.jpg"/></coverpage>'
            # Tiny 1x1 black JPEG in base64
            tiny_jpg_b64 = "/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAP//////////////////////////////////////////////////////////////////////////////////////wgALCAABAAEBAREA/8QAFBABAAAAAAAAAAAAAAAAAAAAAP/aAAgBAQABPxA="
            binary_xml = f'<binary id="cover.jpg" content-type="image/jpeg">{tiny_jpg_b64}</binary>'

        return f"""<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <genre>{book['genres'][0]}</genre>
      {author_xml}
      <book-title>{book['title']}</book-title>
      {annotation_xml}
      {keywords_xml}
      {cover_xml}
      <lang>{book['lang']}</lang>
    </title-info>
  </description>
  <body><p>Sample content for {book['title']}</p></body>
  {binary_xml}
</FictionBook>"""

def main():
    parser = argparse.ArgumentParser(description="Librium Data Generator")
    parser.add_argument("--out", help="Output directory", default=str(CPaths.OUT_DIR / "test_lib"))
    parser.add_argument("--books", type=int, default=10, help="Number of random books to generate")
    parser.add_argument("--archive-size", type=int, default=10, help="Number of books per ZIP archive")
    parser.add_argument("--covers", action="store_true", help="Generate covers for books")
    args = parser.parse_args()

    CUI.banner(f"GENERATING TEST LIBRARY: {args.books} books ({args.archive_size} per archive)")
    gen = CLibraryGenerator(args.out)
    
    # Realistic Author -> Titles mapping
    library_data = {
        "Tolstoy,Leo,Nikolaevich": ["War and Peace", "Anna Karenina", "Resurrection", "The Death of Ivan Ilyich"],
        "Orwell,George,": ["1984", "Animal Farm", "Homage to Catalonia", "Down and Out in Paris and London"],
        "Fitzgerald,F. Scott,": ["The Great Gatsby", "Tender Is the Night", "This Side of Paradise"],
        "Melville,Herman,": ["Moby Dick", "Typee", "Omoo", "Billy Budd"],
        "Tolkien,J.R.R.,": ["The Hobbit", "The Fellowship of the Ring", "The Two Towers", "The Return of the King"],
        "Huxley,Aldous,": ["Brave New World", "Island", "Point Counter Point"],
        "Herbert,Frank,": ["Dune", "Dune Messiah", "Children of Dune"],
        "Bradbury,Ray,": ["Fahrenheit 451", "The Martian Chronicles", "Dandelion Wine"],
        "Heller,Joseph,": ["Catch-22", "Closing Time", "Something Happened"],
        "Lee,Harper,": ["To Kill a Mockingbird", "Go Set a Watchman"],
        "Стругацкий,Аркадий,Натанович": ["Пикник на обочине", "Трудно быть богом", "Понедельник начинается в субботу", "Улитка на склоне"],
        "Стругацкий,Борис,Натанович": ["Град обреченный", "За миллиард лет до конца света", "Отягощенные злом"],
        "Asimov,Isaac,": ["Foundation", "I, Robot", "The Caves of Steel"],
        "Gibson,William,": ["Neuromancer", "Count Zero", "Mona Lisa Overdrive"],
        "Weir,Andy,": ["The Martian", "Project Hail Mary", "Artemis"],
        "Dostoevsky,Fyodor,Mikhailovich": ["Crime and Punishment", "The Brothers Karamazov", "The Idiot", "The Gambler"],
        "Bulgakov,Mikhail,Afanasyevich": ["The Master and Margarita", "Heart of a Dog", "The White Guard"],
        "Pushkin,Alexander,Sergeevich": ["Eugene Onegin", "The Captain's Daughter", "Ruslan and Ludmila"],
        "Gogol,Nikolai,Vasilyevich": ["Dead Souls", "The Overcoat", "The Nose", "Taras Bulba"],
        "Chekhov,Anton,Pavlovich": ["The Cherry Orchard", "Three Sisters", "Uncle Vanya"],
        "Lem,Stanislaw,": ["Solaris", "The Cyberiad", "The Invincible"],
        "Clarke,Arthur,C.": ["2001: A Space Odyssey", "Rendezvous with Rama", "Childhood's End"]
    }
    
    authors_list = list(library_data.keys())
    
    archive_idx = 1
    for i in range(args.books):
        # Pick a random author and one of their books
        author = random.choice(authors_list)
        title = random.choice(library_data[author])
        
        # e.g., fb2-000001-000010.zip
        archive_name = f"fb2-{archive_idx:06d}-{archive_idx+args.archive_size-1:06d}.zip"
        if (i + 1) % args.archive_size == 0:
            archive_idx += args.archive_size
            
        gen.add_book(
            title=f"{title} vol.{i}",
            authors=author,
            rating=random.randint(1, 5),
            annotation=f"Randomly generated annotation for book {i}.",
            archive=archive_name,
            has_cover=args.covers
        )
    
    path = gen.generate()
    CUI.info(f"Library generated at: {path}")

if __name__ == "__main__":
    main()
