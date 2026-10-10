package com.crayonengine.player;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * CrayonActivity loads libSDL3.so and libmain.so, requests storage permissions,
 * and passes the target game project/script path to SDL_main().
 */
public class CrayonActivity extends SDLActivity {
    private static final String TAG = "Crayon";
    private String mGamePath = null;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        checkStoragePermissions();
        resolveGamePathFromIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        resolveGamePathFromIntent(intent);
    }

    private void checkStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + getPackageName()));
                    startActivity(intent);
                } catch (Exception e) {
                    try {
                        Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                        startActivity(intent);
                    } catch (Exception ignored) {}
                }
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                }, 100);
            }
        }
    }

    private void resolveGamePathFromIntent(Intent intent) {
        if (intent == null) return;

        // 1. Extra "game" passed via adb: am start -e game /sdcard/Crayon/mygame
        if (intent.hasExtra("game")) {
            mGamePath = intent.getStringExtra("game");
            Log.i(TAG, "Game path from intent extra: " + mGamePath);
            return;
        }

        // 2. File opened via Intent (e.g. tapping .crayonproj or .lua in file manager)
        Uri uri = intent.getData();
        if (uri != null) {
            if ("file".equalsIgnoreCase(uri.getScheme())) {
                mGamePath = uri.getPath();
                Log.i(TAG, "Game path from file URI: " + mGamePath);
            } else if ("content".equalsIgnoreCase(uri.getScheme())) {
                String realPath = getPathFromContentUri(uri);
                if (realPath != null && new File(realPath).exists()) {
                    mGamePath = realPath;
                    Log.i(TAG, "Game path resolved from content URI: " + mGamePath);
                } else {
                    mGamePath = copyContentUriToCache(uri);
                    Log.i(TAG, "Game file copied from content URI to: " + mGamePath);
                }
            }
        }
    }

    private String getPathFromContentUri(Uri uri) {
        try {
            String path = uri.getPath();
            if (path != null) {
                if (path.contains("/storage/emulated/0/")) {
                    return path.substring(path.indexOf("/storage/emulated/0/"));
                }
                if (path.contains("/sdcard/")) {
                    return path.substring(path.indexOf("/sdcard/"));
                }
                String docId = uri.getLastPathSegment();
                if (docId != null && docId.startsWith("primary:")) {
                    return Environment.getExternalStorageDirectory().getAbsolutePath() + "/" + docId.substring(8);
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to parse content uri path", e);
        }
        return null;
    }

    private String copyContentUriToCache(Uri uri) {
        try {
            InputStream in = getContentResolver().openInputStream(uri);
            if (in == null) return null;
            File outDir = getExternalFilesDir(null);
            if (outDir == null) outDir = getFilesDir();
            File cacheFile = new File(outDir, "temp_opened_script.lua");
            FileOutputStream out = new FileOutputStream(cacheFile);
            byte[] buf = new byte[8192];
            int read;
            while ((read = in.read(buf)) != -1) {
                out.write(buf, 0, read);
            }
            in.close();
            out.close();
            return cacheFile.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "Failed to copy content URI", e);
            return null;
        }
    }

    @Override
    protected String[] getArguments() {
        if (mGamePath != null && !mGamePath.isEmpty()) {
            return new String[] { "--game", mGamePath };
        }
        return new String[0];
    }
}
