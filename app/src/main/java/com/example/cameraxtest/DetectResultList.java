package com.example.cameraxtest;

public class DetectResultList
{
    private int id;                     // 对应 int id
    private int count;                  // 对应 int count
    private DetectResult[] results;     // 对应 object_detect_result 数组

    public DetectResultList(int id, int count, DetectResult[] results)
    {
        this.id = id;
        this.count = count;
        this.results = results;
    }

    // getter（按需添加）
    public int getId() { return id; }
    public int getCount() { return count; }
    public DetectResult[] getResults() { return results; }
}
