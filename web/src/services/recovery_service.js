// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Recovery & Autosave Service
// Distinguishes unavailable browser/storage capability from an empty recovery list.
// Never invents recovery snapshots or fabricates recovery success.

export const SESSION_MARKER_KEY = "fe3d.stage1.session_marker.v1";
const HUB_DB_NAME = "fe3d_stage1_hub_db_v1";
const RECOVERY_STORE = "recovery_snapshots";
export const MAX_RECOVERY_RING_SLOTS = 3;

function safeWindowLocalStorage() {
  try {
    return typeof window !== "undefined" ? window.localStorage : null;
  } catch (_) {
    return null;
  }
}

function safeWindowIndexedDb() {
  try {
    return typeof indexedDB !== "undefined" ? indexedDB : null;
  } catch (_) {
    return null;
  }
}

export class RecoveryService {
  constructor(options = {}) {
    this.storage =
      options.storage !== undefined ? options.storage : safeWindowLocalStorage();
    this.idbFactory =
      options.idbFactory !== undefined ? options.idbFactory : safeWindowIndexedDb();
    this.forceUnavailable = Boolean(
      options.forceUnavailable ||
        (typeof window !== "undefined" && window.__FE3D_FORCE_RECOVERY_UNAVAILABLE__)
    );
    this.useMemoryBackend = Boolean(options.useMemoryBackend);
    this.nowFn = options.nowFn || (() => Date.now());

    this.storageState = "uninitialized"; // "available" | "unavailable" | "uninitialized"
    this.unavailableReason = "";
    this.hadUncleanPreviousSession = false;
    this.nextSlotIndex = 0;
    this.snapshotCounter = 0;
    this.snapshots = [];
    this.memorySnapshots = new Map();
    this.listeners = new Set();
  }

  async initialize() {
    this.snapshots = [];
    this.hadUncleanPreviousSession = false;

    if (this.forceUnavailable || (!this.idbFactory && !this.useMemoryBackend)) {
      this.storageState = "unavailable";
      this.unavailableReason =
        "Persistent browser recovery storage (IndexedDB) is unavailable or disabled in this browsing context.";
      this._notify();
      return this.getStatus();
    }

    try {
      if (this.storage) {
        const rawMarker = this.storage.getItem(SESSION_MARKER_KEY);
        if (rawMarker) {
          const parsed = JSON.parse(rawMarker);
          if (parsed && parsed.sessionActive === true) {
            this.hadUncleanPreviousSession = true;
          }
          if (Number.isFinite(Number(parsed?.nextSlotIndex))) {
            this.nextSlotIndex = Math.max(0, Number(parsed.nextSlotIndex)) % MAX_RECOVERY_RING_SLOTS;
          }
          if (Number.isFinite(Number(parsed?.snapshotCounter))) {
            this.snapshotCounter = Math.max(0, Number(parsed.snapshotCounter));
          }
        }
      }
    } catch (_) {}

    const loaded = await this._readAllFromBackend();
    if (loaded === null) {
      this.storageState = "unavailable";
      this.unavailableReason =
        "IndexedDB recovery database could not be opened (storage quota or private-mode restriction).";
      this._notify();
      return this.getStatus();
    }

    this.snapshots = loaded
      .filter((r) => r && typeof r.jsonText === "string" && r.jsonText.length > 0)
      .map((r) => ({
        ...r,
        uncleanSession: Boolean(r.uncleanSession || this.hadUncleanPreviousSession),
      }))
      .sort((a, b) => b.capturedAt - a.capturedAt);

    this.storageState = "available";
    this.unavailableReason = "";
    this._writeMarker(true);

    if (this.hadUncleanPreviousSession && this.snapshots.length > 0) {
      for (const rec of this.snapshots) {
        await this._putToBackend(rec);
      }
    }

    this._notify();
    return this.getStatus();
  }

  getStatus() {
    return {
      storageState: this.storageState,
      unavailableReason: this.unavailableReason,
      hadUncleanPreviousSession: this.hadUncleanPreviousSession,
      maxRingSlots: MAX_RECOVERY_RING_SLOTS,
      snapshots: this.snapshots.map(({ jsonText, ...meta }) => ({ ...meta })),
    };
  }

  async captureSnapshot({
    projectName = "Untitled Project",
    author = "Final Effect 3D",
    originPath = "",
    jsonText = "",
    objectCount = 0,
    reason = "Periodic Autosave",
  } = {}) {
    if (this.storageState !== "available") {
      return {
        ok: false,
        error: this.unavailableReason || "Recovery storage is unavailable.",
      };
    }
    if (typeof jsonText !== "string" || jsonText.trim().length === 0) {
      return {
        ok: false,
        error: "Cannot capture an empty project snapshot.",
      };
    }

    const slotIndex = this.nextSlotIndex % MAX_RECOVERY_RING_SLOTS;
    this.nextSlotIndex = (slotIndex + 1) % MAX_RECOVERY_RING_SLOTS;
    const seq = ++this.snapshotCounter;
    const id = `slot_${slotIndex}`;

    const record = {
      id,
      sequence: seq,
      slotIndex,
      projectName: String(projectName || "Untitled Project"),
      author: String(author || "Final Effect 3D"),
      originPath: String(originPath || ""),
      capturedAt: this.nowFn(),
      byteSize: jsonText.length,
      objectCount: Math.max(0, Number(objectCount) || 0),
      reason: String(reason || "Periodic Autosave"),
      uncleanSession: false,
      jsonText,
    };

    const wrote = await this._putToBackend(record);
    if (!wrote) {
      return {
        ok: false,
        error: "Failed to persist autosave snapshot to recovery storage.",
      };
    }

    this.snapshots = [record, ...this.snapshots.filter((s) => s.id !== id)].sort(
      (a, b) => b.capturedAt - a.capturedAt
    );
    this._writeMarker(true);
    this._notify();

    const { jsonText: _omitted, ...publicRecord } = record;
    return { ok: true, snapshot: publicRecord };
  }

  async getSnapshotPayload(id) {
    if (this.storageState !== "available") return null;
    const target =
      !id || id === "latest"
        ? this.snapshots[0]
        : this.snapshots.find((s) => s.id === id);
    return target ? target : null;
  }

  async discardSnapshot(id) {
    if (this.storageState !== "available") return false;
    const exists = this.snapshots.some((s) => s.id === id);
    if (!exists) return false;
    await this._deleteFromBackend(id);
    this.snapshots = this.snapshots.filter((s) => s.id !== id);
    if (this.snapshots.length === 0) {
      this.hadUncleanPreviousSession = false;
    }
    this._notify();
    return true;
  }

  async discardAll() {
    if (this.storageState !== "available") return false;
    for (const s of this.snapshots) {
      await this._deleteFromBackend(s.id);
    }
    this.snapshots = [];
    this.hadUncleanPreviousSession = false;
    this._notify();
    return true;
  }

  markCleanShutdown() {
    this.hadUncleanPreviousSession = false;
    this._writeMarker(false);
  }

  subscribe(listener) {
    if (typeof listener !== "function") return () => {};
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  _writeMarker(sessionActive) {
    try {
      if (this.storage) {
        this.storage.setItem(
          SESSION_MARKER_KEY,
          JSON.stringify({
            sessionActive: Boolean(sessionActive),
            nextSlotIndex: this.nextSlotIndex,
            snapshotCounter: this.snapshotCounter,
            updatedAt: this.nowFn(),
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
          if (!db.objectStoreNames.contains("recent_payloads")) {
            db.createObjectStore("recent_payloads");
          }
          if (!db.objectStoreNames.contains(RECOVERY_STORE)) {
            db.createObjectStore(RECOVERY_STORE, { keyPath: "id" });
          }
        };
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => resolve(null);
      } catch (_) {
        resolve(null);
      }
    });
  }

  async _readAllFromBackend() {
    if (this.useMemoryBackend) {
      return Array.from(this.memorySnapshots.values());
    }
    const db = await this._openDb();
    if (!db) return null;
    return new Promise((resolve) => {
      try {
        const tx = db.transaction(RECOVERY_STORE, "readonly");
        const req = tx.objectStore(RECOVERY_STORE).getAll();
        req.onsuccess = () => {
          const res = Array.isArray(req.result) ? req.result : [];
          db.close();
          resolve(res);
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

  async _putToBackend(record) {
    if (this.useMemoryBackend) {
      this.memorySnapshots.set(record.id, { ...record });
      return true;
    }
    const db = await this._openDb();
    if (!db) return false;
    return new Promise((resolve) => {
      try {
        const tx = db.transaction(RECOVERY_STORE, "readwrite");
        tx.objectStore(RECOVERY_STORE).put(record);
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

  async _deleteFromBackend(id) {
    if (this.useMemoryBackend) {
      this.memorySnapshots.delete(id);
      return;
    }
    const db = await this._openDb();
    if (!db) return;
    await new Promise((resolve) => {
      try {
        const tx = db.transaction(RECOVERY_STORE, "readwrite");
        tx.objectStore(RECOVERY_STORE).delete(id);
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

  _notify() {
    const status = this.getStatus();
    for (const fn of this.listeners) {
      fn(status);
    }
  }
}
