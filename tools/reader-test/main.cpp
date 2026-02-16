#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <EInkDisplay.h>
#include <Epub.h>
#include <EpubChapterParser.h>
#include <Epub/Page.h>
#include <GfxRenderer.h>
#include <Markdown.h>
#include <MarkdownParser.h>
#include <PageCache.h>
#include <PlainTextParser.h>
#include <Epub/RenderConfig.h>
#include <SDCardManager.h>
#include <Txt.h>
#include <LittleFS.h>

// LittleFS global
MockLittleFS LittleFS;

// GfxRenderer static frame buffer
uint8_t GfxRenderer::frameBuffer_[EInkDisplay::BUFFER_SIZE];

enum ContentType { EPUB, MARKDOWN, TXT_FILE, UNKNOWN };

static ContentType detectType(const std::string& path) {
  auto ext = path.substr(path.find_last_of('.') + 1);
  for (auto& c : ext) c = static_cast<char>(tolower(c));
  if (ext == "epub") return EPUB;
  if (ext == "md" || ext == "markdown") return MARKDOWN;
  if (ext == "txt") return TXT_FILE;
  return UNKNOWN;
}

static bool mkdirRecursive(const std::string& path) {
  std::string current;
  for (size_t i = 0; i < path.size(); i++) {
    current += path[i];
    if (path[i] == '/' && i > 0) {
      mkdir(current.c_str(), 0755);
    }
  }
  mkdir(path.c_str(), 0755);
  return true;
}

static void dumpPages(PageCache& cache) {
  for (int p = 0; p < cache.pageCount(); p++) {
    auto page = cache.loadPage(p);
    if (!page) continue;
    printf("    --- Page %d ---\n", p);
    for (auto& elem : page->elements) {
      if (elem->getTag() == TAG_PageLine) {
        auto& tb = static_cast<PageLine*>(elem.get())->getTextBlock();
        for (auto& wd : tb.getWords()) {
          printf("%s ", wd.word.c_str());
        }
        printf("\n");
      }
    }
  }
}

static void usage() {
  fprintf(stderr, "Usage: reader-test [--dump] <file.epub|.md|.txt> [output_dir]\n");
  fprintf(stderr, "  --dump     Print parsed text content of each page\n");
  fprintf(stderr, "  output_dir defaults to /tmp/sumi-cache/\n");
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    usage();
    return 1;
  }

  bool dump = false;
  int argIdx = 1;
  if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
    dump = true;
    argIdx++;
  }

  if (argIdx >= argc) {
    usage();
    return 1;
  }

  const std::string filepath = argv[argIdx];
  const std::string outputDir = argIdx + 1 < argc ? argv[argIdx + 1] : "/tmp/sumi-cache";

  ContentType type = detectType(filepath);
  if (type == UNKNOWN) {
    fprintf(stderr, "Unsupported file type: %s\n", filepath.c_str());
    return 1;
  }

  // Setup renderer with fixed-width font metrics
  EInkDisplay display(0, 0, 0, 0, 0, 0);
  GfxRenderer gfx(display);
  gfx.begin();

  RenderConfig config;
  config.fontId = 0;
  config.viewportWidth = 474;   // 480 - margins
  config.viewportHeight = 788;  // 800 - margins
  config.paragraphAlignment = 0;
  config.spacingLevel = 1;
  config.lineCompression = 1.0f;

  mkdirRecursive(outputDir);

  if (type == EPUB) {
    auto epub = std::make_shared<Epub>(filepath, outputDir);
    if (!epub->load()) {
      fprintf(stderr, "Failed to load EPUB: %s\n", filepath.c_str());
      return 1;
    }
    printf("EPUB: \"%s\" by %s, %d spine items\n", epub->getTitle().c_str(), epub->getAuthor().c_str(),
           epub->getSpineItemsCount());

    std::string sectionsDir = epub->getCachePath() + "/sections";
    mkdirRecursive(sectionsDir);
    std::string imageCachePath = epub->getCachePath() + "/images";

    int totalPages = 0;
    for (int i = 0; i < epub->getSpineItemsCount(); i++) {
      std::string cachePath = sectionsDir + "/" + std::to_string(i) + ".bin";
      EpubChapterParser parser(epub, i, gfx, config, imageCachePath);
      PageCache cache(cachePath);
      cache.create(parser, config, 0);
      printf("  Spine %d: %d pages -> %s\n", i, cache.pageCount(), cachePath.c_str());
      if (dump) dumpPages(cache);
      totalPages += cache.pageCount();
    }
    printf("Total: %d pages\n", totalPages);

  } else if (type == MARKDOWN) {
    Markdown md(filepath, outputDir);
    if (!md.load()) {
      fprintf(stderr, "Failed to load Markdown: %s\n", filepath.c_str());
      return 1;
    }
    printf("Markdown: \"%s\"\n", md.getTitle().c_str());

    MarkdownParser parser(filepath, gfx, config);
    std::string cachePath = outputDir + "/pages_0.bin";
    PageCache cache(cachePath);
    cache.create(parser, config, 0);
    printf("Markdown: %d pages -> %s\n", cache.pageCount(), cachePath.c_str());
    if (dump) dumpPages(cache);

  } else {
    Txt txt(filepath, outputDir);
    if (!txt.load()) {
      fprintf(stderr, "Failed to load TXT: %s\n", filepath.c_str());
      return 1;
    }
    printf("TXT: \"%s\"\n", txt.getTitle().c_str());

    PlainTextParser parser(filepath, gfx, config);
    std::string cachePath = outputDir + "/pages_0.bin";
    PageCache cache(cachePath);
    cache.create(parser, config, 0);
    printf("TXT: %d pages -> %s\n", cache.pageCount(), cachePath.c_str());
    if (dump) dumpPages(cache);
  }

  return 0;
}
