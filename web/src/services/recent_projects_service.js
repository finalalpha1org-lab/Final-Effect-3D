// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Recent Projects Service
// Tracks real opened/saved .fe3d project metadata, real paths, optional genuine viewport
// thumbnails, and browser-accessible payloads/handles. Never fabricates entries or previews.

export const RECENT_INDEX_STORAGE_KEY = "fe3d.stage1.recent_index.v1";
const HUB_DB_NAME = "fe3d_stage1_hub_db_v1";
const PAYLOAD_STORE = "recent_payloads";

function deterministicId(seedText) {
  let h = 2166136261 >>> 0;
  const s = String(seedText || "");
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 16777619) >>> 0;
  }
  return "proj_" + h.toString(16).padStart(8, "0");
}

function normalizeVirtualPath(rawPath, fallbackStem) {
  const cleaned = String(rawPath || "")
    .trim()
    .replace(/\\/g, "/")
    .replace(/\/+/g, "/");
  if (cleaned) return cleaned;
  const stem = String(fallbackStem || "Untitled_Project")
    .trim()
    .replace(/[^a-zA-Z0-9._-]+/g, "_") || "Untitled_Project";
  return `/workspace/projects/${stem}.fe3d`;
}

function isGenuineDataThumbnail(dataUrl) {
  return (
    typeof dataUrl === "string" &&
    (dataUrl.startsWith("data:image/jpeg;base64,") ||
      dataUrl.startsWith("data:image/png;base64,") ||
      dataUrl.startsWith("data:image/webp;base64,")) &&
    dataUrl.length > 64
  );
}

export class RecentProjectsService {
  constructor(options = {}) {
    this.storageKey = options.storageKey || RECENT_INDEX_STORAGE_KEY;
    this.storage =
      options.storage !== undefined
        ? options.storage
        : typeof window !== "undefined"
        ? window.localStorage
        : null;
    this.idbFactory =
      options.idbFactory !== undefined
        ? options.idbFactory
        : typeof indexedDB !== "undefined"
        ? indexedDB
        : null;
    this.nowFn = options.nowFn || (() => Date.now());
    this.memoryPayloads = new Map();
    this.fileHandles = new Map();
    this.listeners = new Set();
    this.entries = this._loadIndex();
  }

  _loadIndex() {
    try {
      if (!this.storage) return [];
      const raw = this.storage.getItem(this.storageKey);
      if (!raw) return [];
      const parsed = JSON.parse(raw);
      const list = Array.isArray(parsed) ? parsed : Array.isArray(parsed?.entries) ? parsed.entries : [];
      return list
        .filter((e) => e && typeof e === "object" && typeof e.name === "string" && e.name.trim())
        .map((e) => ({
          id: String(e.id || deterministicId(e.filePath || e.name)),
          name: String(e.name).trim(),
          author: String(e.author || "Final Effect 3D"),
          sceneName: String(e.sceneName || "Scene"),
          filePath: normalizeVirtualPath(e.filePath, e.name),
          formatLabel: String(e.formatLabel || ".fe3d v1.3"),
          byteSize: Number.isFinite(Number(e.byteSize)) ? Math.max(0, Number(e.byteSize)) : 0,
          objectCount: Number.isFinite(Number(e.objectCount)) ? Math.max(0, Number(e.objectCount)) : 0,
          meshCount: Number.isFinite(Number(e.meshCount)) ? Math.max(0, Number(e.meshCount)) : 0,
          materialCount: Number.isFinite(Number(e.materialCount)) ? Math.max(0, Number(e.materialCount)) : 0,
          updatedAt: Number.isFinite(Number(e.updatedAt)) ? Number(e.updatedAt) : 0,
          pinned: Boolean(e.pinned),
          storageKind: String(e.storageKind || "browser_workspace"),
          available: e.available !== false,
          thumbnailDataUrl: isGenuineDataThumbnail(e.thumbnailDataUrl) ? e.thumbnailDataUrl : null,
        }));
    } catch (_) {
      return [];
    }
  }

  _saveIndex() {
    try {
      if (this.storage) {
        this.storage.setItem(
          this.storageKey,
          JSON.stringify({
            format: "finaleffect.recent_projects",
            version: 1,
            entries: this.entries,
          })
        );
      }
    } catch (_) {}
  }

  _openDb() {
    return new Promise((resolve) => {
      if (!this.idbFactory) {
        resolve(null);
        return;
      }
      try {
        const req = this.idbFactory.open(HUB_DB_NAME, 1);
        req.onupgradeneeded = () => {
          const db = req.result;
          if (!db.objectStoreNames.contains(PAYLOAD_STORE)) {
            db.createObjectStore(PAYLOAD_STORE);
          }
          if (!db.objectStoreNames.contains("recovery_snapshots")) {
            db.createObjectStore("recovery_snapshots", { keyPath: "id" });
          }
        };
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => resolve(null);
      } catch (_) {
        resolve(null);
      }
    });
  }

  async _writePayload(id, jsonText) {
    if (typeof jsonText !== "string" || !jsonText) return false;
    this.memoryPayloads.set(id, jsonText);
    const db = await this._openDb();
    if (!db) return true;
    return new Promise((resolve) => {
      try {
        const tx = db.transaction(PAYLOAD_STORE, "readwrite");
        tx.objectStore(PAYLOAD_STORE).put(jsonText, id);
        tx.oncomplete = () => {
          db.close();
          resolve(true);
        };
        tx.onerror = () => {
          db.close();
          resolve(false);
        };
      } catch (_) {
        db.close();
        resolve(false);
      }
    });
  }

  async loadPayload(id) {
    if (this.memoryPayloads.has(id)) {
      return this.memoryPayloads.get(id);
    }
    const db = await this._openDb();
    if (!db) return null;
    return new Promise((resolve) => {
      try {
        const tx = db.transaction(PAYLOAD_STORE, "readonly");
        const req = tx.objectStore(PAYLOAD_STORE).get(id);
        req.onsuccess = () => {
          const val = typeof req.result === "string" ? req.result : null;
          if (val) this.memoryPayloads.set(id, val);
          db.close();
          resolve(val);
        };
        req.onerror = () => {
          db.close();
          resolve(null);
        };
      } catch (_) {
        db.close();
        resolve(null);
      }
    });
  }

  async _deletePayload(id) {
    this.memoryPayloads.delete(id);
    this.fileHandles.delete(id);
    const db = await this._openDb();
    if (!db) return;
    await new Promise((resolve) => {
      try {
        const tx = db.transaction(PAYLOAD_STORE, "readwrite");
        tx.objectStore(PAYLOAD_STORE).delete(id);
        tx.oncomplete = () => {
          db.close();
          resolve();
        };
        tx.onerror = () => {
          db.close();
          resolve();
        };
      } catch (_) {
        db.close();
        resolve();
      }
    });
  }

  findByPath(targetPath) {
    const norm = normalizeVirtualPath(targetPath, "").toLowerCase();
    return this.entries.find((e) => e.filePath.toLowerCase() === norm) || null;
  }

  findById(id) {
    return this.entries.find((e) => e.id === id) || null;
  }

  getFileHandle(id) {
    return this.fileHandles.get(id) || null;
  }

  async recordProject(meta = {}, jsonPayload = null, maxUnpinned = 20) {
    const name = String(meta.name || "Untitled Project").trim() || "Untitled Project";
    const filePath = normalizeVirtualPath(meta.filePath, name);
    const id = String(meta.id || deterministicId(filePath));
    const prev = this.entries.find((e) => e.id === id || e.filePath.toLowerCase() === filePath.toLowerCase());

    const byteSize =
      Number.isFinite(Number(meta.byteSize)) && Number(meta.byteSize) > 0
        ? Number(meta.byteSize)
        : typeof jsonPayload === "string"
        ? jsonPayload.length
        : prev?.byteSize || 0;

    const thumbnailDataUrl = isGenuineDataThumbnail(meta.thumbnailDataUrl)
      ? meta.thumbnailDataUrl
      : prev?.thumbnailDataUrl || null;

    const entry = {
      id: prev ? prev.id : id,
      name,
      author: String(meta.author || prev?.author || "Final Effect 3D"),
      sceneName: String(meta.sceneName || prev?.sceneName || "Scene"),
      filePath,
      formatLabel: String(meta.formatLabel || ".fe3d v1.3"),
      byteSize,
      objectCount: Number.isFinite(Number(meta.objectCount)) ? Math.max(0, Number(meta.objectCount)) : 0,
      meshCount: Number.isFinite(Number(meta.meshCount)) ? Math.max(0, Number(meta.meshCount)) : 0,
      materialCount: Number.isFinite(Number(meta.materialCount)) ? Math.max(0, Number(meta.materialCount)) : 0,
      updatedAt: this.nowFn(),
      pinned: prev ? Boolean(prev.pinned) : Boolean(meta.pinned),
      storageKind: String(meta.storageKind || (meta.fileHandle ? "native_handle" : "browser_workspace")),
      available: true,
      thumbnailDataUrl,
    };

    if (meta.fileHandle) {
      this.fileHandles.set(entry.id, meta.fileHandle);
    }

    this.entries = [
      entry,
      ...this.entries.filter(
        (e) => e.id !== entry.id && e.filePath.toLowerCase() !== filePath.toLowerCase()
      ),
    ];
    this.enforceLimit(maxUnpinned);

    if (typeof jsonPayload === "string" && jsonPayload.length > 0) {
      await this._writePayload(entry.id, jsonPayload);
    }

    this._saveIndex();
    this._notify();
    return entry;
  }

  markAvailability(id, available) {
    let changed = false;
    for (const item of this.entries) {
      if (item.id === id && item.available !== Boolean(available)) {
        item.available = Boolean(available);
        changed = true;
      }
    }
    if (changed) {
      this._saveIndex();
      this._notify();
    }
  }

  togglePin(id) {
    const target = this.entries.find((e) => e.id === id);
    if (!target) return false;
    target.pinned = !target.pinned;
    this._saveIndex();
    this._notify();
    return target.pinned;
  }

  async removeProject(id) {
    const before = this.entries.length;
    this.entries = this.entries.filter((e) => e.id !== id);
    await this._deletePayload(id);
    if (this.entries.length !== before) {
      this._saveIndex();
      this._notify();
      return true;
    }
    return false;
  }

  async clearUnpinned() {
    const toRemove = this.entries.filter((e) => !e.pinned);
    this.entries = this.entries.filter((e) => e.pinned);
    for (const item of toRemove) {
      await this._deletePayload(item.id);
    }
    this._saveIndex();
    this._notify();
    return toRemove.length;
  }

  enforceLimit(maxUnpinned = 20) {
    const cap = Math.max(1, Number(maxUnpinned) || 20);
    let unpinnedSeen = 0;
    const kept = [];
    for (const item of this.entries) {
      if (item.pinned) {
        kept.push(item);
      } else if (unpinnedSeen < cap) {
        unpinnedSeen++;
        kept.push(item);
      }
    }
    this.entries = kept;
  }

  query({ search = "", sort = "recent", filter = "all" } = {}) {
    const needle = String(search || "").trim().toLowerCase();
    const filtered = this.entries.filter((item) => {
      if (filter === "pinned" && !item.pinned) return false;
      if (filter === "available" && !item.available) return false;
      if (!needle) return true;
      return (
        item.name.toLowerCase().includes(needle) ||
        item.filePath.toLowerCase().includes(needle) ||
        item.author.toLowerCase().includes(needle) ||
        item.sceneName.toLowerCase().includes(needle)
      );
    });

    filtered.sort((a, b) => {
      if (a.pinned !== b.pinned) return a.pinned ? -1 : 1;
      if (sort === "name") {
        const cmp = a.name.localeCompare(b.name, undefined, { sensitivity: "base" });
        if (cmp !== 0) return cmp;
      } else if (sort === "size") {
        if (b.byteSize !== a.byteSize) return b.byteSize - a.byteSize;
      }
      return b.updatedAt - a.updatedAt;
    });

    return filtered.map((item) => ({ ...item }));
  }

  count() {
    return this.entries.length;
  }

  subscribe(listener) {
    if (typeof listener !== "function") return () => {};
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  _notify() {
    const snapshot = this.query();
    for (const fn of this.listeners) {
      fn(snapshot);
    }
  }
}
