# Librium Web Interface

The **Librium Web Interface** is a modern, responsive Single Page Application (SPA) designed for easy home use. It provides a visual way to browse, search, and download books from your INPX collection.

---

## 1. Architecture

The web part follows a **Proxy-Backend** pattern to bridge the C++ Engine's TCP protocol with standard web technologies.

```text
┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
│   Browser (Frontend)   │ ◄──► │   Node.js (Backend)    │ ◄──► │   Librium.exe (C++)    │
│   Vanilla JS / CSS     │ HTTP │   Express / Net        │ TCP  │   Engine Core          │
└────────────────────────┘      └────────────────────────┘      └────────────────────────┘
```

### Components
- **Frontend**: A clean Single Page Application built with Vanilla JavaScript and CSS.
  - **Grid View**: Displays book covers in a responsive grid.
  - **Infinite Scroll**: Loads books automatically as you scroll down using `IntersectionObserver`.
  - **Search Bar**: Supports filtering by Title, Author, Series, Genre, and Language.
  - **Book Modal**: Shows full book metadata, large cover (correctly sized), annotation, and a download button.
  - **Author Display**: Intelligent formatting (e.g., "Author A, Author B" or "Author A + N" for multiple authors).
  - **Progress Overlay**: Appears during initial import or manual library rebuilds.
- **Backend (Node.js)**:
  - **Process Management**: Automatically spawns and monitors the `Librium.exe` process (requires Node.js >= 18).
  - **TCP Bridge**: Handles a persistent connection to the C++ Engine and manages a request queue with timeouts.
  - **LRU Cover Cache**: Stores up to 500 recently accessed covers in RAM (supports JPG, PNG, and WebP).
  - **Security**: Validates all incoming book IDs and sanitizes query parameters to prevent attacks.
  - **Static Server**: Serves the frontend files and cover images from the `meta/` directory.

---

## 2. Directory Structure

```text
web/
├── public/                # Static frontend assets
│   ├── index.html         # Main SPA layout
│   ├── style.css          # Dark-themed styles (Plex/Calibre inspired)
│   └── app.js             # Frontend logic & state management
├── server.js              # Node.js Express server & TCP bridge
├── package.json           # Node dependencies (Express)
└── web_config.json        # Configuration (generated automatically)
```

---

## 3. Performance & Optimization

### LRU In-Memory Caching
To handle large libraries (e.g., 60GB of covers) without overwhelming the client or the server's disk:
- **Server-Side**: The Node.js proxy maintains a **Least Recently Used (LRU)** cache in RAM for covers. Only the most active covers are kept in memory.
- **Client-Side**: Covers are served with a `Cache-Control: max-age=3600` header (1 hour), balancing responsiveness with local storage usage on mobile devices.

### Smart Pagination
The frontend requests books in batches of 40. The `IntersectionObserver` pre-fetches the next batch before the user reaches the bottom, ensuring a smooth "infinite" scrolling experience.

---

## 4. Usage & Deployment

### Local Development / Windows
The easiest way to run the web interface is using the provided Python automation script:

```powershell
python scripts/RunWeb.py --preset x64-release --library "C:\Path\To\Your\Library"
```

**What this script does:**
1. Builds the C++ Engine in release mode.
2. Detects your `.inpx` and archives folder.
3. Generates necessary JSON configs in `out/artifacts/web/`.
4. Installs `npm` dependencies.
5. Launches the web server at `http://localhost:8080`.

### Initial Import
On the first launch, if the database is empty, the interface will automatically trigger a full library import. A progress bar will be displayed. You can also manually trigger an incremental update using the **"Обновить" (Update)** button in the header.

---

## 5. Maintenance

- **Artifacts**: All web-related data (database, logs, temporary exports, covers) are stored in `out/artifacts/web/`.
- **Resetting**: To start fresh, simply delete the `out/artifacts/web/` directory.
- **Logs**: Backend logs are printed to the console; C++ Engine logs are written to `out/artifacts/web/librium.log`.

---

## 6. Automated Testing

The web interface includes a backend API test suite to ensure security and reliability.

### Key Features
- **Framework**: Built with **Jest** and **Supertest**.
- **Engine Mocking**: Uses a lightweight mock TCP server to simulate the C++ Engine, allowing tests to run instantly without compiling the main project.
- **Full Isolation**: Tests are executed in an ephemeral environment within `out/artifacts/<preset>/web_test/`. This keeps the `web/` source directory clean and prevents dependency conflicts.
- **Clean Lifecycle**: Implements explicit resource cleanup (sockets, servers, timers) to ensure 100% reliable test execution.

### Running Tests
To run all web-related tests, use the master automation script:

```powershell
python scripts/Run.py test --stage web
```

This will automatically prepare the environment, install dependencies, and execute the test suite.
