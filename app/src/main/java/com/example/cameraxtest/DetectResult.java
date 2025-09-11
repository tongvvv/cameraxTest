package com.example.cameraxtest;

public class DetectResult
{
    private ImageRect box;       // 对应 image_rect_t box
    private float prop;          // 对应 float prop
    private int clsId;           // 对应 int cls_id

    public DetectResult(ImageRect box, float prop, int clsId)
    {
        this.box = box;
        this.prop = prop;
        this.clsId = clsId;
    }

    // getter（按需添加）
    public ImageRect getBox() { return box; }
    public float getProp() { return prop; }
    public int getClsId() { return clsId; }
}
