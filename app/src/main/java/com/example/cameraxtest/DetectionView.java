package com.example.cameraxtest;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;

import java.util.Locale;

// 自定义绘制视图
public class DetectionView extends View
{
    private DetectResultList mResultList;
    private Paint mBoxPaint;
    private Paint mTextPaint;

    private float mScale; //缩放比例(预览尺寸/原始尺寸)

    public DetectionView(Context context)
    {
        super(context);
        initPaints();
    }

    public void setScale(int imgW, int viewW)
    {
        mScale =  viewW/(float)imgW;
    }

    private void initPaints()
    {
        // 初始化矩形框画笔
        mBoxPaint = new Paint();
        mBoxPaint.setColor(Color.GREEN);
        mBoxPaint.setStyle(Paint.Style.STROKE);
        mBoxPaint.setStrokeWidth(3);

        // 初始化文字画笔
        mTextPaint = new Paint();
        mTextPaint.setColor(Color.RED);
        mTextPaint.setTextSize(30);
    }

    // 设置检测结果
    public void setResults(DetectResultList results)
    {
        mResultList = results;
        invalidate(); // 触发重绘
    }

    @Override
    protected void onDraw(Canvas canvas)
    {
        super.onDraw(canvas);
        if (mResultList == null) return;

        // 遍历所有检测结果并绘制
        for (int i = 0; i < mResultList.getCount(); i++)
        {
            DetectResult result = mResultList.getResults()[i];

            // 原始坐标
            float originalLeft = result.getBox().getLeft();
            float originalTop = result.getBox().getTop();
            float originalRight = result.getBox().getRight();
            float originalBottom = result.getBox().getBottom();

            // 转换为预览区域坐标（乘以缩放比例）
            float previewLeft = originalLeft * mScale;
            float previewTop = originalTop * mScale;
            float previewRight = originalRight * mScale;
            float previewBottom = originalBottom * mScale;

            // 绘制矩形框（使用转换后的坐标）
            canvas.drawRect(previewLeft, previewTop, previewRight, previewBottom, mBoxPaint);

            // 绘制类别和概率（文字位置也需要转换）
            String text = clsToName.getName(result.getClsId()) + " " +
                    String.format(Locale.CHINA, "%.2f", result.getProp());
            canvas.drawText(text, previewLeft, (previewTop-5) > 0? (previewTop-5): previewTop+25, mTextPaint);
        }
    }
}
