#pragma once
namespace qbot {
    enum FileType
    {
        picture = 1,
        video = 2,
        voice = 3,
        file = 4
    };
    static constexpr auto FILE_POS_MD5_10M = 10002432;
}