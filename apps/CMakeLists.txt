# Apps directory - includes all app subdirectories

# For iOS builds, only build LayerCake (iPad-only app)
if(IOS)
    add_subdirectory(layercake)
else()
    add_subdirectory(basic)
    add_subdirectory(text2sound)
    add_subdirectory(text2sound4all)
    add_subdirectory(embeddingsampler)
    add_subdirectory(layercake)
endif()

