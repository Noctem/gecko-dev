/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "SiteDataTestUtils",
];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {ContentTask} = ChromeUtils.import("resource://testing-common/ContentTask.jsm");
const {BrowserTestUtils} = ChromeUtils.import("resource://testing-common/BrowserTestUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "swm",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

XPCOMUtils.defineLazyGlobalGetters(this, ["indexedDB", "Blob"]);

/**
 * This module assists with tasks around testing functionality that shows
 * or clears site data.
 *
 * Please note that you will have to clean up changes made manually, for
 * example using SiteDataTestUtils.clear().
 */
var SiteDataTestUtils = {

  /**
   * Makes an origin have persistent data storage.
   *
   * @param {String} origin - the origin of the site to give persistent storage
   *
   * @returns a Promise that resolves when storage was persisted
   */
  persist(origin, value = Services.perms.ALLOW_ACTION) {
    return new Promise(resolve => {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
      Services.perms.addFromPrincipal(principal, "persistent-storage", value);
      Services.qms.persist(principal).callback = () => resolve();
    });
  },

  /**
   * Adds a new blob entry to a dummy indexedDB database for the specified origin.
   *
   * @param {String} origin - the origin of the site to add test data for
   * @param {Number} size [optional] - the size of the entry in bytes
   *
   * @returns a Promise that resolves when the data was added successfully.
   */
  addToIndexedDB(origin, size = 1024) {
    return new Promise(resolve => {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
      let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
      request.onupgradeneeded = function(e) {
        let db = e.target.result;
        db.createObjectStore("TestStore");
      };
      request.onsuccess = function(e) {
        let db = e.target.result;
        let tx = db.transaction("TestStore", "readwrite");
        let store = tx.objectStore("TestStore");
        tx.oncomplete = resolve;
        let buffer = new ArrayBuffer(size);
        let blob = new Blob([buffer]);
        store.add(blob, Cu.now());
      };
    });
  },

  /**
   * Adds a new cookie for the specified origin, with the specified contents.
   * The cookie will be valid for one day.
   *
   * @param {String} origin - the origin of the site to add test data for
   * @param {String} name [optional] - the cookie name
   * @param {String} value [optional] - the cookie value
   */
  addToCookies(origin, name = "foo", value = "bar") {
    let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
    Services.cookies.add(principal.URI.host, principal.URI.pathQueryRef, name, value,
      false, false, false, Date.now() + 24000 * 60 * 60, {},
      Ci.nsICookie2.SAMESITE_UNSET);
  },

  /**
   * Adds a new serviceworker with the specified path. Note that this
   * method will open a new tab at the domain of the SW path to that effect.
   *
   * @param {String} path - the path to the service worker to add.
   *
   * @returns a Promise that resolves when the service worker was registered
   */
  addServiceWorker(path) {
    let uri = Services.io.newURI(path);
    // Register a dummy ServiceWorker.
    return BrowserTestUtils.withNewTab(uri.prePath, async function(browser) {
      return ContentTask.spawn(browser, {path}, async ({path: p}) => {
        // eslint-disable-next-line no-undef
        let r = await content.navigator.serviceWorker.register(p);
        return new Promise(resolve => {
          let worker = r.installing;
          worker.addEventListener("statechange", () => {
            if (worker.state === "installed") {
              resolve();
            }
          });
        });
      });
    });
  },

  _getCacheStorage(where, lci) {
    switch (where) {
      case "disk": return Services.cache2.diskCacheStorage(lci, false);
      case "memory": return Services.cache2.memoryCacheStorage(lci);
      case "appcache": return Services.cache2.appCacheStorage(lci, null);
      case "pin": return Services.cache2.pinningCacheStorage(lci);
    }
    return null;
  },

  hasCacheEntry(path, where, lci = Services.loadContextInfo.default) {
    let storage = this._getCacheStorage(where, lci);
    return storage.exists(Services.io.newURI(path), "");
  },

  addCacheEntry(path, where, lci = Services.loadContextInfo.default) {
    return new Promise(resolve => {
      function CacheListener() { }
      CacheListener.prototype = {
        QueryInterface: ChromeUtils.generateQI(["nsICacheEntryOpenCallback"]),

        onCacheEntryCheck(entry, appCache) {
          return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
        },

        onCacheEntryAvailable(entry, isnew, appCache, status) {
          resolve();
        },
      };

      let storage = this._getCacheStorage(where, lci);
      storage.asyncOpenURI(Services.io.newURI(path), "", Ci.nsICacheStorage.OPEN_NORMALLY, new CacheListener());
    });
  },

  /**
   * Checks whether the specified origin has registered ServiceWorkers.
   *
   * @param {String} origin - the origin of the site to check
   *
   * @returns {Boolean} whether or not the site has ServiceWorkers.
   */
  hasServiceWorkers(origin) {
    let serviceWorkers = swm.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      if (sw.principal.origin == origin) {
        return true;
      }
    }
    return false;
  },

  /**
   * Gets the current quota usage for the specified origin.
   *
   * @returns a Promise that resolves to an integer with the total
   *          amount of disk usage by a given origin.
   */
  getQuotaUsage(origin) {
    return new Promise(resolve => {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
      Services.qms.getUsageForPrincipal(principal, request => resolve(request.result.usage));
    });
  },

  /**
   * Cleans up all site data.
   */
  clear() {
    return new Promise(resolve => {
      Services.clearData.deleteData(
        Ci.nsIClearDataService.CLEAR_COOKIES |
        Ci.nsIClearDataService.CLEAR_ALL_CACHES |
        Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES |
        Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
        Ci.nsIClearDataService.CLEAR_PREDICTOR_NETWORK_DATA |
        Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
        Ci.nsIClearDataService.CLEAR_EME |
        Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS, resolve);
    });
  },
};
