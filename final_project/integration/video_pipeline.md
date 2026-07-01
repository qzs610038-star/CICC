# Video Pipeline

正式视频链路：

```text
video_in -> raw_unpack -> debayer -> wb_gamma -> roi_crop -> feature_extract -> osd -> dvi_tx
```

后续需补充每一级的分辨率、像素格式、时钟、复位、valid/de/hs/vs 语义和双通道对应关系。
