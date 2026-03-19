#!/usr/bin/env python3
import os
import zipfile
import uuid
import random
from datetime import datetime

class LibGen:
    """Generator of realistic test data for Librium."""
    
    SEP = "\x04"
    
    def __init__(self, output_dir):
        self.output_dir = output_dir
        self.lib_path = os.path.join(output_dir, "test_library")
        os.makedirs(self.lib_path, exist_ok=True)
        self.books = []
        self.archives = {}
        self.next_id = 100001

    def add_book(self, title, authors, genres=None, lang="ru", deleted=False, 
                 series="", ser_no="", archive="fb2-001.zip", lib_id=None,
                 annotation=None, keywords="", size=None, date=None, rating=0,
                 raw_fb2=None):
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
            "raw_fb2": raw_fb2
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
            arch_path = os.path.join(self.lib_path, arch_name)
            with zipfile.ZipFile(arch_path, "w", zipfile.ZIP_DEFLATED) as zf:
                for b in books:
                    fb2_content = b['raw_fb2'] if b['raw_fb2'] else self._generate_fb2(b)
                    zf.writestr(f"{b['file']}.fb2", fb2_content)

        # 2. Create INPX
        inpx_path = os.path.join(self.lib_path, "library.inpx")
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
        
        return self.lib_path

    def _generate_fb2(self, book):
        annotation_xml = f"<annotation><p>{book['annotation']}</p></annotation>" if book['annotation'] else ""
        keywords_xml = f"<keywords>{book['keywords']}</keywords>" if book['keywords'] else ""
        
        return f"""<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description>
    <title-info>
      <genre>{book['genres'][0]}</genre>
      <author><last-name>{book['authors'][0]}</last-name></author>
      <book-title>{book['title']}</book-title>
      {annotation_xml}
      {keywords_xml}
      <lang>{book['lang']}</lang>
    </title-info>
  </description>
  <body><p>Sample content for {book['title']}</p></body>
</FictionBook>"""

if __name__ == "__main__":
    import sys
    gen = LibGen(sys.argv[1] if len(sys.argv) > 1 else "./out/test_lib")
    gen.add_book("War and Peace", "Tolstoy,Leo,Nikolaevich", annotation="Great novel")
    gen.add_book("The Shining", "King,Stephen", lang="en")
    print(f"Library generated at: {gen.generate()}")
