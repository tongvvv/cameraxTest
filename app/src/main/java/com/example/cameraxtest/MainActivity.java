package com.example.cameraxtest;

import static android.view.Surface.ROTATION_90;

import androidx.camera.camera2.interop.Camera2Interop;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.Preview;
import androidx.camera.lifecycle.ProcessCameraProvider;

import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;

import static androidx.camera.core.ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import androidx.camera.core.Camera;
import androidx.camera.core.ImageProxy;
import androidx.camera.core.Preview;
import androidx.camera.core.ResolutionInfo;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.view.PreviewView;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.LifecycleOwner;

import android.Manifest;
import android.annotation.SuppressLint;

import android.content.pm.PackageManager;
import android.graphics.ImageFormat;

import android.media.Image;
import android.os.Bundle;
import android.util.Log;
import android.util.Range;
import android.util.Size;
import android.view.Surface;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.example.cameraxtest.databinding.ActivityMainBinding;
import com.google.common.util.concurrent.ListenableFuture;

import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity
{
    static
    {
        System.loadLibrary("cameraxtest"); // 对应你的 .so 库名
    }

    private ListenableFuture<ProcessCameraProvider> cameraProviderFuture;
    private PreviewView previewView;
    private ProcessCameraProvider cameraProvider;
    private FrameLayout frameLayout;

    DetectionView detectionView;

    private int counttest = 0;
    private long timetest = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        previewView=findViewById(R.id.previewView);//初始化
        frameLayout=findViewById(R.id.frameLayout);

        detectionView = new DetectionView(this);
        frameLayout.addView(detectionView);

        String modelPath = AssetFileCopier.copyAssetToInternalStorage(this, "yolov8.rknn");
        String labelPath = AssetFileCopier.copyAssetToInternalStorage(this, "coco_80_labels_list.txt");
        Log.e("MODEL",modelPath);
        Log.e("LABEL",labelPath);
        if(!nativePrepare(modelPath, labelPath))
        {
            Log.e("prepare","nativePrepare failed");
        }

        //高版本系统动态权限申请
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) ==
                PackageManager.PERMISSION_DENIED)
        {
            requestPermissions(new String[]{Manifest.permission.CAMERA,}, 11);
        }
        else
        {
            //启动相机
            startCamera();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String[] permissions,
                                           @NonNull int[] grantResults)
    {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode==11){//获取权限后，开启摄像头
            //启动相机
            startCamera();
        }
    }

    private void startCamera()
    {
        // 请求 CameraProvider
        cameraProviderFuture = ProcessCameraProvider.getInstance(this);
        //检查 CameraProvider 可用性，验证它能否在视图创建后成功初始化
        cameraProviderFuture.addListener(() ->
        {
            try
            {
                ProcessCameraProvider cameraProvider = cameraProviderFuture.get();
                bindPreview(cameraProvider);
            }
            catch (ExecutionException | InterruptedException e)
            {
                // No errors need to be handled for this Future.
                // This should never be reached.
            }
        }, ContextCompat.getMainExecutor(this));
    }

    //选择相机并绑定生命周期和用例
    private void bindPreview(@NonNull ProcessCameraProvider cp)
    {
        this.cameraProvider=cp;

        //必须有这一行旋转才生效
        previewView.setImplementationMode(PreviewView.ImplementationMode.COMPATIBLE);

        Preview preview = new Preview.Builder()
                .setTargetResolution(new Size(1280, 1024))   //比例: 5:4 帧率: 30FPS
                .setTargetRotation(Surface.ROTATION_90)
                .build();


        @SuppressLint("UnsafeOptInUsageError")
        CameraSelector cameraSelector = new CameraSelector.Builder()
                .requireLensFacing(CameraSelector.LENS_FACING_EXTERNAL)//CameraSelector.LENS_FACING_EXTERNAL
                .build();

        preview.setSurfaceProvider(previewView.getSurfaceProvider());

        // 创建图像分析用例来获取相机数据格式
        ImageAnalysis imageAnalysis = new ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setTargetResolution(new Size(1280, 1024))
                .build();

        //不填参数的话, 默认格式YUV_420_888, 对于此相机来说, 具体是yuv420sp nv12.
        imageAnalysis.setAnalyzer(Executors.newSingleThreadExecutor(), image -> {
            // 获取图像格式信息
            int format = image.getFormat();
            String formatName = getFormatName(format);

            ImageProxy.PlaneProxy[] planeProxies = image.getPlanes();

            String TAG = "CameraFormat";

            // 获取图像宽高
            int width = image.getWidth();
            int height = image.getHeight();

            ByteBuffer imgBuf = planeProxies[0].getBuffer();

            long start = System.nanoTime();
            DetectResultList resultList = imgInference(imgBuf, width, height);
            long end   = System.nanoTime();

            if(resultList != null)
            {
                detectionView.setScale(image.getWidth(), frameLayout.getWidth());
                detectionView.setResults(resultList);
            }

            counttest++;
            timetest += end-start;
            if(counttest%50 == 0)
            {
                //预处理约2.7ms 推理约25.6ms 后处理约2.4ms(置信度0.25时)
                timetest /= 50; //五十次的平均值
                Log.e("TIME", "图片处理平均耗时: " + timetest/1000000.0 + "毫秒");
                timetest = 0;
            }

            image.close();
        });

        cameraProvider.unbindAll();//解绑组件
        Camera camera = cameraProvider.bindToLifecycle((LifecycleOwner) this, cameraSelector, preview, imageAnalysis);
    }

    // 辅助方法：将图像格式代码转换为可读名称
    private String getFormatName(int format) {
        switch (format) {
            case ImageFormat.JPEG: return "JPEG";
            case ImageFormat.YUV_420_888: return "YUV_420_888";
            case ImageFormat.YUV_422_888: return "YUV_422_888";
            case ImageFormat.YUV_444_888: return "YUV_444_888";
            case ImageFormat.RAW_SENSOR: return "RAW_SENSOR";
            case ImageFormat.RAW10: return "RAW10";
            case ImageFormat.RAW12: return "RAW12";
            default: return "UNKNOWN (" + format + ")";
        }
    }

    @Override
    protected void onDestroy()
    {
        super.onDestroy();
        nativeDestroy();
        cameraProvider.unbindAll();
    }

    private native DetectResultList imgInference(ByteBuffer img, int width, int height);
    private native boolean nativePrepare(String path, String labelPath);
    private native void nativeDestroy();
}