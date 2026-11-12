#pragma once
namespace qbot {
    enum FileType
    {
        picture = 1,
        video = 2,
        voice = 3,
        file = 4
    };
    enum SceneType
    {
        c2c,
        group,
        guild
    };
    enum MsgType
    {
        content = 0,
        markdown = 2,
        input_notify = 6,
        media = 7,
        card = 8
    };
    constexpr auto FILE_POS_MD5_10M = 10002432;
    constexpr auto MEDIA_IMAGE_MAX_SIZE = 10 * 1024 * 1024;
}