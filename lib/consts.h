#pragma once

#include <cassert>
#include <cstddef>
#include "string.h"


// Logging (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=INSTR, 5=NONE)
constexpr uint8_t LOG_LEVEL = 2;
constexpr const char* USER_AGENT = "Seamus the Search Engine (web crawler for university course)";

// Global
constexpr size_t NUM_MACHINES = 3;
constexpr const char* MACHINES[NUM_MACHINES] = {"34.55.208.73", "34.44.13.147", "34.30.238.129"};

inline const char* get_machine_addr(size_t machine_id) {
    assert(machine_id < NUM_MACHINES);
    return MACHINES[machine_id];
}

inline size_t my_machine_id() {
    const char* env = std::getenv("MACHINE_ID");
    assert(env != nullptr && "MACHINE_ID environment variable is not set");
    return std::strtoul(env, nullptr, 10);
}


// Crawler
constexpr uint16_t CRAWLER_LISTENER_PORT = 8000;
constexpr size_t CRAWLER_LISTENER_THREADS = 16;
constexpr size_t CRAWLER_THREADPOOL_SIZE = 1<<11;
constexpr size_t CRAWLER_CAROUSEL_SIZE = CRAWLER_THREADPOOL_SIZE*16;
static constexpr size_t CRAWLER_CAROUSEL_QUEUE_SIZE = 32;
constexpr size_t CRAWLER_MAX_QUEUE_SIZE = 32;
static_assert(CRAWLER_CAROUSEL_SIZE % CRAWLER_THREADPOOL_SIZE == 0, "[consts.h]: CRAWLER_CAROUSEL_SIZE must be a multiple of CRAWLER_THREADPOOL_SIZE");
static_assert(CRAWLER_CAROUSEL_SIZE >= CRAWLER_THREADPOOL_SIZE, "[consts.h]: CRAWLER_THREADPOOL_SIZE cannot be greater than CRAWLER_CAROUSEL_SIZE");

constexpr size_t CRAWLER_BACKOFF_SEC = 3;                   // Time (seconds) to wait between sending GET requests to URLs within the same domain carousel slot
constexpr size_t CRAWLER_PERSIST_INTERVAL_SEC = 60;         // Time (seconds) to wait between persists of in-memory priority buckets -> disk priority bucket files
constexpr size_t CRAWLER_FEED_INTERVAL_SEC = 1;             // Time (seconds) to wait between feeding in-memory priority buckets -> domain carousel
constexpr size_t CRAWLER_WORKER_SLEEP_MS = 10;              // Time (milliseconds) for the crawler worker to sleep before moving to a new slot
constexpr size_t CRAWLER_INSTRUMENTATION_INTERVAL_SEC = 20; // Time (seconds) between instrumentation drain cycles
constexpr size_t CRAWLER_INSTRUMENTATION_BATCH_SIZE = 8192;    // Number of successful crawls before submitting a batched metric update

constexpr size_t CRAWLER_OUTBOUND_BATCH_SIZE = 1<<13;         // Number of crawl targets to buffer per machine before sending
constexpr size_t PRIORITY_BUCKETS = 8;
constexpr size_t NUM_PARSERS = CRAWLER_THREADPOOL_SIZE;
static_assert(NUM_PARSERS == CRAWLER_THREADPOOL_SIZE);      // TODO(hershey): make sure this assumption is valid


// Seed URLs (loaded into priority bucket 0 on startup)
constexpr int SEED_LIST_SIZE = 262;
 
constexpr const char* SEED_LIST[SEED_LIST_SIZE] = {
    "https://www.ed.gov/",
    "https://www.energy.gov/national-laboratories",
    "https://www.loc.gov/",
    "https://www.justice.gov/",
    "https://www.nih.gov/",
    "https://www.whitehouse.gov/",
    "https://www.nasa.gov/",
    "https://www.noaa.gov/",
    "https://www.cdc.gov/",
    "https://www.fda.gov/",
    "https://www.usgs.gov/",
    "https://www.census.gov/",
    "https://www.bls.gov/",
    "https://www.federalreserve.gov/",
    "https://home.treasury.gov/",
    "https://www.state.gov/",
    "https://www.defense.gov/",
    "https://www.supremecourt.gov/",
    "https://www.senate.gov/",
    "https://www.house.gov/",
    "https://www.usa.gov/",
    "https://www.si.edu/",
    "https://www.gov.uk/",
    "https://european-union.europa.eu/",
    "https://www.un.org/",
    "https://www.who.int/",
    "https://www.worldbank.org/",
    "https://www.imf.org/",
    "https://www.berkeley.edu/",
    "https://www.cam.ac.uk/",
    "https://www.uchicago.edu/en",
    "https://www.cmu.edu/",
    "https://www.cornell.edu/",
    "https://home.dartmouth.edu/",
    "https://www.duke.edu/",
    "https://www.harvard.edu/",
    "https://www.jhu.edu/",
    "https://umich.edu/",
    "https://www.mit.edu/",
    "https://www.northwestern.edu/",
    "https://www.ox.ac.uk/",
    "https://www.princeton.edu/",
    "https://www.stanford.edu/",
    "https://www.ucla.edu/",
    "https://www.unc.edu/",
    "https://www.virginia.edu/",
    "https://www.yale.edu/",
    "https://www.caltech.edu/",
    "https://www.columbia.edu/",
    "https://www.upenn.edu/",
    "https://www.brown.edu/",
    "https://www.nyu.edu/",
    "https://www.washington.edu/",
    "https://www.utexas.edu/",
    "https://www.gatech.edu/",
    "https://www.wisc.edu/",
    "https://www.imperial.ac.uk/",
    "https://www.lse.ac.uk/",
    "https://www.utoronto.ca/",
    "https://www.mcgill.ca/",
    "https://ethz.ch/en.html",
    "https://www.vogue.com/",
    "https://www.gq.com/",
    "https://www.harpersbazaar.com/",
    "https://www.elle.com/",
    "https://www.esquire.com/",
    "https://www.businessoffashion.com/",
    "https://www.fandom.com/",
    "https://www.imdb.com/",
    "https://www.rottentomatoes.com/",
    "https://www.metacritic.com/",
    "https://letterboxd.com/",
    "https://variety.com/",
    "https://www.hollywoodreporter.com/",
    "https://deadline.com/",
    "https://www.indiewire.com/",
    "https://www.criterion.com/",
    "https://www.bfi.org.uk/",
    "https://tvtropes.org/",
    "https://www.allrecipes.com/",
    "https://www.eater.com/",
    "https://www.food.com/",
    "https://www.foodnetwork.com/",
    "https://www.seriouseats.com/",
    "https://www.bonappetit.com/",
    "https://cooking.nytimes.com/",
    "https://www.epicurious.com/",
    "https://www.kingarthurbaking.com/",
    "https://smittenkitchen.com/",
    "https://www.bbcgoodfood.com/",
    "https://guide.michelin.com/",
    "https://www.goodreads.com/",
    "https://lithub.com/",
    "https://www.poetryfoundation.org/",
    "https://www.theparisreview.org/",
    "https://www.nybooks.com/",
    "https://www.lrb.co.uk/",
    "https://www.gutenberg.org/",
    "https://openlibrary.org/",
    "https://www.kirkusreviews.com/",
    "https://www.publishersweekly.com/",
    "https://genius.com/",
    "https://pitchfork.com/",
    "https://www.rollingstone.com/",
    "https://www.billboard.com/",
    "https://www.npr.org/music/",
    "https://www.allmusic.com/",
    "https://www.discogs.com/",
    "https://musicbrainz.org/",
    "https://www.stereogum.com/",
    "https://consequence.net/",
    "https://theathletic.com/",
    "https://www.cbssports.com/",
    "https://www.espn.com/",
    "https://www.foxsports.com/",
    "https://bleacherreport.com/",
    "https://www.si.com/",
    "https://sports.yahoo.com/",
    "https://www.bbc.com/sport",
    "https://www.skysports.com/",
    "https://www.baseball-reference.com/",
    "https://www.basketball-reference.com/",
    "https://www.pro-football-reference.com/",
    "https://arxiv.org/",
    "https://scholar.google.com/",
    "https://www.jstor.org/",
    "https://pubmed.ncbi.nlm.nih.gov/",
    "https://www.semanticscholar.org/",
    "https://www.ssrn.com/",
    "https://www.biorxiv.org/",
    "https://www.medrxiv.org/",
    "https://plos.org/",
    "https://www.researchgate.net/",
    "https://doaj.org/",
    "https://core.ac.uk/",
    "https://www.britannica.com/",
    "https://www.merriam-webster.com/",
    "https://www.oed.com/",
    "https://en.wikipedia.org/",
    "https://dictionary.cambridge.org/",
    "https://www.collinsdictionary.com/",
    "https://www.dictionary.com/",
    "https://www.thesaurus.com/",
    "https://www.etymonline.com/",
    "https://plato.stanford.edu/",
    "https://iep.utm.edu/",
    "https://www.history.com/",
    "https://www.nationalgeographic.com/",
    "https://www.nature.com/",
    "https://www.science.org/",
    "https://www.sciencedaily.com/",
    "https://www.ted.com/",
    "https://www.wikihow.com/",
    "https://www.khanacademy.org/",
    "https://www.coursera.org/",
    "https://www.edx.org/",
    "https://ocw.mit.edu/",
    "https://www.scientificamerican.com/",
    "https://www.newscientist.com/",
    "https://www.quantamagazine.org/",
    "https://www.smithsonianmag.com/",
    "https://bigthink.com/",
    "https://aeon.co/",
    "https://news.google.com/",
    "https://www.msn.com/",
    "https://www.yahoo.com/",
    "https://www.apple.com/apple-news/",
    "https://flipboard.com/",
    "https://www.reddit.com/",
    "https://stackexchange.com/",
    "https://stackoverflow.com/",
    "https://www.youtube.com/",
    "https://www.quora.com/",
    "https://news.ycombinator.com/",
    "https://www.metafilter.com/",
    "https://abcnews.go.com/",
    "https://apnews.com/",
    "https://www.bbc.com/",
    "https://www.cbsnews.com/",
    "https://www.cnn.com/",
    "https://www.cnbc.com/",
    "https://www.economist.com/",
    "https://www.nbcnews.com/",
    "https://www.npr.org/",
    "https://www.nytimes.com/",
    "https://www.newyorker.com/",
    "https://www.pbs.org/newshour/",
    "https://www.reuters.com/",
    "https://time.com/",
    "https://www.wsj.com/",
    "https://www.washingtonpost.com/",
    "https://www.michigandaily.com/",
    "https://www.theguardian.com/",
    "https://www.usatoday.com/",
    "https://www.bloomberg.com/",
    "https://www.ft.com/",
    "https://www.theatlantic.com/",
    "https://www.axios.com/",
    "https://www.vox.com/",
    "https://www.aljazeera.com/",
    "https://www.propublica.org/",
    "https://www.thetimes.co.uk/",
    "https://www.independent.co.uk/",
    "https://www.cbc.ca/news",
    "https://www.investopedia.com/",
    "https://www.marketwatch.com/",
    "https://www.forbes.com/",
    "https://fortune.com/",
    "https://www.barrons.com/",
    "https://www.morningstar.com/",
    "https://seekingalpha.com/",
    "https://www.fool.com/",
    "https://www.factcheck.org/",
    "https://www.politifact.com/",
    "https://www.snopes.com/",
    "https://apnews.com/hub/ap-fact-check",
    "https://www.reuters.com/fact-check/",
    "https://fullfact.org/",
    "https://thehill.com/",
    "https://www.foreignaffairs.com/",
    "https://www.politico.com/",
    "https://www.wired.com/",
    "https://foreignpolicy.com/",
    "https://rollcall.com/",
    "https://www.realclearpolitics.com/",
    "https://www.brookings.edu/",
    "https://www.cfr.org/",
    "https://www.theverge.com/",
    "https://arstechnica.com/",
    "https://techcrunch.com/",
    "https://www.engadget.com/",
    "https://www.technologyreview.com/",
    "https://spectrum.ieee.org/",
    "https://www.theregister.com/",
    "https://www.404media.co/",
    "https://www.tomshardware.com/",
    "https://www.anandtech.com/",
    "https://www.expedia.com/",
    "https://www.tripadvisor.com/",
    "https://www.yelp.com/",
    "https://www.booking.com/",
    "https://www.consumerreports.org/",
    "https://www.nytimes.com/wirecutter/",
    "https://www.rtings.com/",
    "https://www.indeed.com/",
    "https://www.glassdoor.com/",
    "https://commons.wikimedia.org/",
    "https://www.w3schools.com/",
    "https://en.wikisource.org/",
    "https://en.wiktionary.org/",
    "https://en.wikiquote.org/",
    "https://en.wikivoyage.org/",
    "https://www.linkedin.com/",
    "https://github.com/",
    "https://gitlab.com/",
    "https://developer.mozilla.org/",
    "https://archive.org/",
    "https://archive.ph/",
    "https://www.pewresearch.org/",
    "https://news.gallup.com/",
    "https://www.statista.com/",
    "https://ourworldindata.org/",
};


// Parser
static constexpr size_t MAX_PARSED_PAGES = (3*1e8)/18; // 300M/18
static constexpr const char* PARSER_OUTPUT_DIR = "/var/seamus/parser_output";
static constexpr int MAX_CONSECUTIVE_NON_ALNUM = 100;
static constexpr char RETURN_DELIM = '\r';
static constexpr char NULL_DELIM = '\0';
static constexpr char SPACE_DELIM = ' ';
static constexpr int MAX_LINK_MEMORY = 8 * 1024;
static constexpr int MAX_TITLELEN_MEMORY = 8 * 1024;   // Just copying value for link memory -- no logic behind this
static constexpr int MAX_WORD_MEMORY = 32 * 1024;
static constexpr size_t MAX_BASE_LEN = 256;
static constexpr size_t MAX_HTML_SIZE = 100 * 1024; // 100 KB

// URL Store
static constexpr uint32_t MAX_STORE_URLS = (9*1e8)/18; // 900M/18
static constexpr const char* URL_STORE_OUTPUT_DIR = "/var/seamus/urlstore_output";
static const string URL_STORE_OUTPUT_DIR_STR = string(URL_STORE_OUTPUT_DIR);
constexpr bool URL_FROM_SCRATCH = true; // whether to read from file or start from scratch on url_store bottup
constexpr uint32_t URL_STORE_NUM_THREADS = 16;
                                                            // We cannot have multiple client listeners running concurrently calling read and update methods without locks
                                                            // At the same time, we do want multiple listeners, so we should add some locking mechanism better than a global one
constexpr uint32_t URL_STORE_PORT = 9000;
constexpr uint32_t URL_STORE_MAX_URL_LEN = 4096;           // 4 KB max url length
constexpr uint32_t URL_STORE_MAX_ANCHOR_TEXT_LEN = 512;     // 0.5 KB max anchor text length
constexpr size_t URL_NUM_SHARDS = 64;
constexpr size_t MAX_ANCHORS_PER_URL = 64; // Arbitrary cap to prevent memory blowup from urls with too many anchors -- can be tuned based on observed distribution of anchors per url

// Robots.txt Manager
constexpr size_t ROBOTS_CACHE_SIZE = 1024;


// HTML Server
constexpr uint16_t HTMLSERVER_PORT = 8080;
constexpr size_t HTMLSERVER_THREADS = 8;


// Indexer
constexpr size_t DOCS_PER_INDEX_CHUNK = 500000;
constexpr uint32_t INDEX_SKIP_SIZE = 500;
static constexpr const char* INDEX_OUTPUT_DIR = "/var/seamus/index_output";
constexpr size_t NUM_INDEXER_THREADS = 16; // Should be then number of cores     // todo(Aiden): change depending on number of cores we end up renting per machine



typedef uint32_t Unicode;
typedef uint8_t  Utf8;
typedef uint16_t Utf16;
//
// Byte Order Marks (BOMs)
//
// Utf16 text files are are required to start with a Byte Order Mark (BOM) to
// indicate whether they are big-endian (high byte first) or little-endian
// (low byte first).
//
// If you read a correct BOM as the first 16-bit character in a file, it confirms
// you are reading it with the correct "endian-ness".  All Windows and Apple
// computers are little-endian, so that is what we will support.
const Unicode  ByteOrderMark = 0xfeff;
// If it's actually a big endian file, that first 16-bit character will have
// the two bytes flipped.  (To read big-endian text, simply flip the bytes
// before decoding.)
const Unicode  BigEndianBOM = 0xfffe;
// Utf8 text files do not need a BOM because they're written as a sequence
// of bytes.  Adding BOM to the start of a Utf8 file is permitted but
// discouraged.

// The Utf8 byte order mark is the same 0xfeff Unicode character value
// but written out as Utf8.
const Utf8     Utf8BOMString[ ] = { 0xef, 0xbb, 0xbf };
