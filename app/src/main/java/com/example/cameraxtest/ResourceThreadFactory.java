package com.example.cameraxtest;

import android.util.Log;

import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;

class ResourceWorker extends Thread
{
    private final Runnable task;

    DataProvider  mDataProvider;
    public ResourceWorker(Runnable task, DataProvider dataProvider)
    {
        this.task = task;
        mDataProvider = dataProvider;
    }

    @Override
    public void run()
    {
        try {
            // 线程创建时申请资源（仅执行一次）
            MainActivity.nativePrepare(mDataProvider.getModelPath(), mDataProvider.getLabelPath());
            System.out.println("线程[" + getId() + "]初始化，资源申请完成");

            // 执行线程池分配的任务（线程池会自动循环分配新任务）
            task.run();
        } finally {
            // 线程销毁时释放资源（核心线程仅在被回收时执行）
            MainActivity.nativeDestroy();
        }
    }
}
public class ResourceThreadFactory implements ThreadFactory
{
    private final AtomicInteger threadNum = new AtomicInteger(1);

    private final DataProvider mDataProvider;

    public ResourceThreadFactory(DataProvider dataProvider)
    {
        this.mDataProvider = dataProvider;
    }

    @Override
    public Thread newThread(Runnable r)
    {
        // 包装任务到自定义线程
        Thread thread = new ResourceWorker(r, mDataProvider);
        thread.setName("resource-thread-" + threadNum.getAndIncrement());
        thread.setDaemon(false);
        return thread;
    }
}
