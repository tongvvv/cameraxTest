package com.example.cameraxtest;

import android.content.Context;
import java.io.*;

/**
 * 通用工具类：将assets目录中的任意文件复制到应用内部存储
 */
public class AssetFileCopier {

    /**
     * 将assets中的文件复制到应用内部存储的files目录
     * @param context 上下文
     * @param assetFileName assets中目标文件的名称（若在子目录下，需传入相对路径，如"config/info.txt"）
     * @return 复制后文件在内部存储的绝对路径，失败则返回null
     */
    public static String copyAssetToInternalStorage(Context context, String assetFileName) {
        // 目标文件路径：内部存储/files目录下，与assets中文件名保持一致
        File targetFile = new File(context.getFilesDir(), assetFileName);

        // 若文件已存在且不为空，直接返回路径
        if (targetFile.exists() && targetFile.length() > 0) {
            return targetFile.getAbsolutePath();
        }

        // 确保父目录存在（如果是子目录下的文件）
        File parentDir = targetFile.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            parentDir.mkdirs(); // 递归创建目录
        }

        // 从assets读取并写入目标文件
        try (InputStream is = context.getAssets().open(assetFileName);
             FileOutputStream fos = new FileOutputStream(targetFile)) {

            byte[] buffer = new byte[4096]; // 适当增大缓冲区提升效率
            int bytesRead;
            while ((bytesRead = is.read(buffer)) != -1) {
                fos.write(buffer, 0, bytesRead);
            }
            return targetFile.getAbsolutePath();
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }
}
