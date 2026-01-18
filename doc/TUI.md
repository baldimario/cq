# cqtui - Terminal User Interface for cq

A full-featured TUI (Text User Interface) for interacting with CSV files using SQL queries. Built with zero external dependencies using low-level terminal control.

## Features

- **Directory-as-Database**: Browse and query all CSV files in a directory
- **Multi-tab Interface**: Open multiple tables and query results simultaneously
- **Interactive Table Viewer**: Navigate large datasets with pagination
- **SQL Query Mode**: Execute queries directly in the TUI
- **Keyboard-Driven**: Efficient navigation with vim-inspired shortcuts

## Building

```bash
make          # Builds both cq and cqtui
make tui      # Builds only cqtui
```

## Usage

```bash
# Launch in current directory
./build/cqtui

# Launch in specific directory
./build/cqtui /path/to/csv/files

# With custom CSV delimiter
./build/cqtui -s ';' data/
```

## Keyboard Shortcuts

### Navigation
- `Arrow Keys` - Move cursor in table / Edit query (in `:` mode)
- `Page Up/Down` - Scroll pages
- `Home/End` - Jump to start/end of table / Query start/end
- `Left/Right` - Scroll columns horizontally (in normal mode)

### Tabs
- `Ctrl+N` - Next tab (works in query mode too)
- `Ctrl+P` - Previous tab (works in query mode too)
- `Ctrl+W` - Close current tab

### Actions
- `:` - Enter query mode (type SQL query and press Enter)
- `t` - Show table list to open CSV files
- `w` - Save/export current table to CSV
- `h` - Show help screen
- `q` or `Ctrl+Q` - Quit

### Query Mode
- `Enter` - Execute query
- `ESC` - Cancel query
- `Arrow Keys` - Move cursor left/right in query text
- `Home/End` - Jump to start/end of query
- `Backspace/Delete` - Edit query text
- `Ctrl+N/Ctrl+P` - Switch tabs while editing query

## Query Mode

Press `:` to enter query mode, then type a SQL query:

```sql
SELECT * FROM 'users.csv' WHERE age > 30
SELECT name, COUNT(*) FROM 'orders.csv' GROUP BY name
```

Query results open in a new tab.

## Architecture

- **Terminal Layer** (`src/tui/terminal.c`): Raw terminal control using termios and ANSI escape codes
- **Core Logic** (`src/tui/tui_core.c`): Application state and main event loop
- **Rendering** (`src/tui/tui_render.c`): UI components (tabs, tables, status bar)
- **Input Handling** (`src/tui/tui_input.c`): Keyboard event processing
- **Workspace** (`src/tui/tui_workspace.c`): Directory scanning for CSV files

## Design Decisions

- **Zero Dependencies**: Built using only standard C library and POSIX APIs
- **Modal Interface**: Inspired by vim - normal mode for navigation, query mode for input
- **Reuses libcq**: All SQL parsing and execution uses existing cq infrastructure
- **Low Memory Footprint**: Uses mmap for CSV files, minimal overhead per tab

## Testing

```bash
./build/test_tui    # Run automated functionality tests
```

## Known Limitations

- Terminal must support ANSI escape codes (most modern terminals)
- Requires at least 80x24 terminal size for optimal experience
- Large CSV files (>1M rows) may have slower navigation
