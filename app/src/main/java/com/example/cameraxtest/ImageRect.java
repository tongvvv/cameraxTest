package com.example.cameraxtest;

public class ImageRect
{
    private int left;
    private int top;
    private int right;
    private int bottom;

    // 构造方法
    public ImageRect(int left, int top, int right, int bottom)
    {
        this.left = left;
        this.top = top;
        this.right = right;
        this.bottom = bottom;
    }

    public int getLeft() { return left; }
    public int getTop() { return top; }
    public int getRight() { return right; }
    public int getBottom() { return bottom; }

}
