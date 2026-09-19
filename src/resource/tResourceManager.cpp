/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2005  by 
and the AA DevTeam (see the file AUTHORS(.txt) in the main source directory)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
  
***************************************************************************

*/

#include "aa_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fstream>
#include <sys/types.h>

#include <libxml/nanohttp.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "tConfiguration.h"
#include "tDict.h"
#include "tDirectories.h"
#include "tResourceManager.h"
#include "tString.h"

#ifdef LIBCURL_PROTOCOL_HTTP
#include <curl/curl.h>

class tCurlGlobal
{
public:
    tCurlGlobal()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    };

    ~tCurlGlobal()
    {
        curl_global_cleanup();
    }
};

class tCurlLocal
{
private:
    CURL* _handle;

public:
    tCurlLocal()
    {
        static tCurlGlobal curlGlobal;
        _handle = curl_easy_init();
    };

    ~tCurlLocal()
    {
        curl_easy_cleanup(_handle);
    }

    operator CURL*()
    {
        return _handle;
    }

    static size_t write_to_ostream(char* data, size_t size, size_t nmemb, void* ostream)
    {
        // Cast the user pointer to an ostream and write the data to it
        static_cast<std::ostream*>(ostream)->write(data, size * nmemb);
        // Return the number of bytes processed
        return size * nmemb;
    }
};
#endif

/***************************************************************
 *          tResourceManager                                   *
 ***************************************************************/

// This is a little ugly, open to suggestions :)
tResourceManager::Reference tResourceManager::__inst = tResourceManager::Reference(new tResourceManager() );
tResourceTypeMap st_resourceList;
tResourceTypeMap* tResourceManager::m_ResourceList = &st_resourceList;

/** The constructor for tResourceManager. */
tResourceManager::tResourceManager() {
    // stub constructor for now
}

tResourceManager::~tResourceManager() { }

/** When you need to carry a local reference for the Resource Manager,
 *  use this method to do so.  It may be more convenient for you thataway.
 */
tResourceManager::Reference tResourceManager::GetResourceManager() {
    if(!__inst) {
        __inst = Reference(new tResourceManager() );
    }

    return Reference(__inst);
}

/** Used by tResourceLoader to register a file loader of some sort.
 */
void tResourceManager::RegisterLoader()
{
}

tResource* tResourceManager::GetResource(const char *file, int typeID)
{
    // stub
    return NULL;
}

/**
 *   Call RegisterResourceType when you want to register a new resource
 *   type.  While that may be obvious, make sure you use the tResourceType
 *   class to describe the resource type.
 *
 *   Ok, fine, there's nothing non-obvious about this method.
 */
int tResourceManager::RegisterResourceType(tResourceType* newType) {
    if(m_ResourceList->find(newType->GetName() ) == m_ResourceList->end() ) {
        m_ResourceList->insert( make_pair( newType->GetName(), newType->Get_reference() ) );
        return 1;
    }
    return 0;
}

//tResource* GetResource(const char *file, int typeID) {
//    // stub
//}

// server determined resource repository
tString & tResourceManager::AccessRepoServer()
{
    static tString resRepoServer("http://resource.armagetronad.net/resource/");
    return resRepoServer;
}
// the nSettingItem is in gStuff.cpp

// client determined resource repository
tString & tResourceManager::AccessRepoClient()
{
    static tString resRepoClient("http://resource.armagetronad.net/resource/");
    return resRepoClient;
}

static tSettingItem<tString> conf_res_repo("RESOURCE_REPOSITORY_CLIENT", tResourceManager::AccessRepoClient());

//! set while running on the background fetch thread: keeps it from writing to
//! the console, which is owned by the game thread
static thread_local bool sr_quietFetch = false;

tResourceManager::Result tResourceManager::FetchURI(const char* URI, std::ostream& o)
{
#ifdef LIBCURL_PROTOCOL_HTTP
    {
        tCurlLocal handle;
        if (nullptr == handle)
            return Result::ERROR_Unknown;

        // Set the URL to request
        curl_easy_setopt(handle, CURLOPT_URL, URI);
        // Set the callback function to handle the response
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &tCurlLocal::write_to_ostream);
        // Set the user pointer to be an ostream to which the response will be written
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &o);
        // activate failure on HTTP errors
        curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
        // activate automatic redirection following
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        // activate SSL verification
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
        // shorten timeouts (10s connect, 30s total)
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
        // set user agent
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "Armagetron Advanced"); // using that instead of the variable progtitle so servers always know what to expect
#ifdef DEBUG
        // more detailed error reporting
        char errbuf[CURL_ERROR_SIZE];
        curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
#endif
        // Perform the request
        CURLcode result = curl_easy_perform(handle);
        // Check the result
        if (result != CURLE_OK)
        {
            long http_code = 0;
            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
            // If the request failed, print an error message
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(result) << std::endl;
            return Result::ERROR_Unknown;
        }

        // Clean up
        curl_easy_cleanup(handle);
    }
#else
#ifdef LIBXML_HTTP_ENABLED
    {
        void* ctxt = NULL;
        int len, rc;

        ctxt = xmlNanoHTTPOpen(URI, NULL);
        if (ctxt == NULL)
        {
            if (!sr_quietFetch)
                con << tOutput("$resource_fetcherror_noconnect", URI);
            return ERROR_Uri;
        }

        if ((rc = xmlNanoHTTPReturnCode(ctxt)) != 200)
        {
            if (!sr_quietFetch)
                con << tOutput(rc == 404 ? "$resource_fetcherror_404" : "$resource_fetcherror", rc);
            return static_cast<tResourceManager::Result>(rc);
        }

        // xmlNanoHTTPFetchContent( ctxt, &buf, &len );
        char buf[10000];
        while ((len = xmlNanoHTTPRead(ctxt, buf, sizeof(buf))) > 0)
        {
            o.write(buf, len);
        }

        xmlNanoHTTPClose(ctxt);
    }
#else
#error libcurl or libxml nanohttp required; configure should have told you. Please file a bug.
    return Result::ERROR_Unknown;
#endif
#endif
    if (!sr_quietFetch)
        con << "OK\n";
    return Result::RESULT_Ok;
}

static int myHTTPFetch(const char* URI, const char* filename, const char* savepath)
{
    con << tOutput("$resource_downloading", URI);
    // con << "Downloading " << URI << "...\n";

    try
    {
        std::ofstream o{savepath};
        tResourceManager::Result ret = tResourceManager::FetchURI(URI, o);
        o.close();
        if (ret == tResourceManager::Result::RESULT_Ok)
            return 0;

        // some error
        remove(savepath);
        return ret;
    }
    catch (...)
    {
        remove(savepath);

        return tResourceManager::Result::ERROR_FileAccess;
    }

    return 0;
}

static int myFetch(const char *URIs, const char *filename, const char *savepath) {
    const char *r = URIs, *p, *n;
    char *u;
    size_t len;
    int rv = -1;
    // r = unprocessed data		p = end-of-item + 1		u = item
    // n = to-be r				len = length of item	savepath = result filepath

    while (r[0] != '\0') {
        while (r[0] == ' ') ++r;			// skip spaces at the start of the item
        (p = strchr(r, ';')) ? 0 : (p = strchr(r, '\0'));
        n = (p[0] == '\0') ? p : (p + 1);	// next item starts after the semicolon
        // NOTE: skip semicolons, *NOT* nulls
        while (p[-1] == ' ' && p > r) --p;			// skip spaces at the end of the item
        if (p > r) {						// skip this for null-length items
            len = (size_t)(p - r);
            u = (char*)malloc((len + 1) * sizeof(char));
            strncpy(u, r, len);
            u[len] = '\0';					// u now contains the individual URI
            rv = myHTTPFetch(u, filename, savepath);	// TODO: handle other protocols?
            free(u);
            if (rv == 0) return 0;		// If successful, return the file retrieved
        }
        r = n;								// move onto the next item
    }

    return rv;	// last error
}

// ---------------------------------------------------------------------------
// Background resource fetching
//
// HTTP fetches must never run on the game thread: the game is single threaded,
// the timeout is half a minute and legacy resources point at hosts that have
// been dead for years. locateResourceCached() only ever hands out files that
// are already on disk; anything missing is queued here and downloaded by a
// worker thread. Downloaded files are written to <file>.part and renamed into
// place, so a partially written file is never picked up, and they land in the
// resource write path, i.e. they are cached across sessions. The worker reports
// completions through consumeFetchCompletions(); the game uses that to reload
// textures and cockpits without ever having blocked.
// ---------------------------------------------------------------------------
namespace
{
    struct sr_FetchRequest
    {
        std::string file;      // cache path relative to the resource directory
        std::string uriList;   // ';' separated URIs, tried in order
        std::string savepath;  // absolute path to write to
    };

    std::mutex sr_fetchMutex;
    std::condition_variable sr_fetchCondition;
    std::deque< sr_FetchRequest > sr_fetchQueue;
    std::set< std::string > sr_fetchPending;
    std::thread sr_fetchThread;
    bool sr_fetchStop = false;
    std::atomic<int> sr_fetchCompleted( 0 );

    //! one quiet HTTP fetch; writes to a temp file first, then renames it
    bool sr_fetchOne( std::string const & uri, std::string const & savepath )
    {
        std::string tmp = savepath + ".part";
        bool ok = false;
        try
        {
            std::ofstream o( tmp.c_str(), std::ios::binary );
            if ( o )
            {
                tResourceManager::Result ret = tResourceManager::FetchURI( uri.c_str(), o );
                o.close();
                if ( ret == tResourceManager::Result::RESULT_Ok )
                {
                    std::remove( savepath.c_str() );
                    ok = ( std::rename( tmp.c_str(), savepath.c_str() ) == 0 );
                }
            }
        }
        catch ( ... )
        {
        }
        if ( !ok )
            std::remove( tmp.c_str() );
        return ok;
    }

    //! tries every URI in the ';' separated list
    bool sr_fetchQueued( sr_FetchRequest const & req )
    {
        sr_quietFetch = true;
        bool ok = false;

        std::string const & list = req.uriList;
        std::string::size_type pos = 0;
        while ( pos <= list.size() )
        {
            std::string::size_type end = list.find( ';', pos );
            std::string uri = list.substr( pos, end == std::string::npos ? std::string::npos : end - pos );
            if ( !uri.empty() && sr_fetchOne( uri, req.savepath ) )
            {
                ok = true;
                break;
            }
            if ( end == std::string::npos )
                break;
            pos = end + 1;
        }

        sr_quietFetch = false;
        return ok;
    }

    void sr_fetchWorker()
    {
        for ( ;; )
        {
            sr_FetchRequest req;
            {
                std::unique_lock< std::mutex > lock( sr_fetchMutex );
                sr_fetchCondition.wait( lock, [] { return sr_fetchStop || !sr_fetchQueue.empty(); } );
                if ( sr_fetchQueue.empty() )
                {
                    if ( sr_fetchStop )
                        return;
                    continue;
                }
                req = sr_fetchQueue.front();
                sr_fetchQueue.pop_front();
            }

            bool ok = sr_fetchQueued( req );

            {
                std::lock_guard< std::mutex > lock( sr_fetchMutex );
                sr_fetchPending.erase( req.file );
            }
            if ( ok )
                sr_fetchCompleted.fetch_add( 1 );
        }
    }

    //! caller must hold sr_fetchMutex
    void sr_startFetchWorker()
    {
        if ( !sr_fetchThread.joinable() )
            sr_fetchThread = std::thread( sr_fetchWorker );
    }
}

tString tResourceManager::locateResourceCached(const char *file, const char *uri) {
    if (!file || file[0] == '\0' || file[0] == '/' || file[0] == '\\')
        return tString();

    // A resource path can carry its URI in parentheses, "name-1.aatex.png(uri)"
    // (see tResourcePath). It has to come off before the file lookups below: a
    // URI contains a colon, which those reject as an absolute path, so every
    // graphic that names a URI failed to be found locally and was dropped.
    tString resourcePath( file );
    tString embeddedUri;
    tString::size_type open = resourcePath.find( '(' );
    if ( open != tString::npos && resourcePath.EndsWith( ")" ) )
    {
        embeddedUri = resourcePath.substr( open + 1, resourcePath.size() - open - 2 );
        resourcePath = resourcePath.substr( 0, open );
    }

    tString filepath = tDirectories::Resource().GetReadPath(resourcePath.c_str());
    if (filepath != "")
        return filepath;

    tString savepath = tDirectories::Resource().GetWritePath(resourcePath.c_str());
    if (savepath == "")
        return tString();

    const char *fetchUri = ( uri && uri[0] ) ? uri : embeddedUri.c_str();
    if ( requestFetch( resourcePath.c_str(), fetchUri, (const char *)savepath ) )
        con << "Fetching " << resourcePath << " in the background...\n";

    return tString();
}

bool tResourceManager::requestFetch(const char *file, const char *uri, const char *savepath) {
    if (!file || file[0] == '\0' || savepath == NULL || savepath[0] == '\0')
        return false;

    // Repositories first, the file's own URI last (legacy URIs point at hosts
    // that have been dead for years, so they are only a last resort).
    tString a_uri;
    if ( AccessRepoServer().Len() > 2 )
        a_uri << AccessRepoServer() << file << ';';

    if ( AccessRepoClient().Len() > 2 && AccessRepoClient() != AccessRepoServer() )
        a_uri << AccessRepoClient() << file << ';';

    if (uri && strcmp("0", uri))
        a_uri << uri << ';';

    std::lock_guard< std::mutex > lock( sr_fetchMutex );
    if ( sr_fetchPending.find( file ) != sr_fetchPending.end() )
        return false; // already queued or in flight

    sr_FetchRequest req;
    req.file = file;
    req.uriList = (const char *)a_uri;
    req.savepath = savepath;

    sr_fetchPending.insert( req.file );
    sr_fetchQueue.push_back( req );
    sr_startFetchWorker();
    sr_fetchCondition.notify_one();
    return true;
}

int tResourceManager::consumeFetchCompletions() {
    return sr_fetchCompleted.exchange( 0 );
}

/*
Allows for the fetching and caching of ressources available on the web,
such as maps (xml), texture (jpg, gif, bmp), sound and models.
Nota: On some forums (such as guru3.sytes.net), it is possible for the
download link not give information about the filename or type, ie:
http://guru3.sytes.net/download.php?id=1191. This is why the filename
parameter is required.
Parameters:
uri: The full uri to obtain the ressource
filename: The filename to use for the local ressource
fullPath: determines the type of the return path
forceFetch: if true, existence in the cache is ignored
Return the path to the resource, relative to resources if fullPath is false, absolute or relative to the working directory otherwise.
NOTE: There must be *at least* one directory level, even if it is ./
*/
tString tResourceManager::locateResource(const char *file, const char *uri, bool fullPath, bool forceFetch) {
    tString filepath, a_uri = tString(), savepath, resourcepath;
    int rv;

    char * to_free = NULL; // string to delete later

    {
        char const *pos, *posb;
        char *nf;
        size_t l;

        // Step 1: If 'file' has an open paren, cut everything after it off
        if ( (pos = strchr(file, '(')) ) {
            l = (size_t)(pos - file);
            nf = (char*)malloc((l + 1) * sizeof(char));
            strncpy(nf, file, l);
            nf[l] = '\0';
            file = nf;
            to_free = nf;
            resourcepath = nf;

            // Step 2: Extract URI, if any
            ++pos;
            if ( (posb = strchr(pos, ')')) ) {
                l = (size_t)(posb - pos);
                nf = (char*)malloc((l + 1) * sizeof(char));
                strncpy(nf, pos, l);
                nf[l] = '\0';
                a_uri << nf << ';';
                free( nf );
            }
        }
        // Otherwise, it's a plain resource path without explicit URL
        else
        {
            resourcepath = file;
        }
    }
    // Validate paths and determine detination savepath
    if (!file || file[0] == '\0') {
        con << tOutput( "$resource_no_filename" );
        free( to_free );
        return (tString) NULL;
    }
    if (file[0] == '/' || file[0] == '\\') {
        con << tOutput( "$resource_abs_path" );
        free( to_free );
        return (tString) NULL;
    }

    if( !forceFetch )
    {
        // Do we have this file locally ?
        filepath = tDirectories::Resource().GetReadPath(file);

        if (filepath != "")
        {
            if ( NULL != to_free )
                free( to_free );
            if ( fullPath )
                return filepath;
            else
                return resourcepath;
        }

        con << tOutput( "$resource_not_cached", file );
    }

    // determine place to save the resource
    savepath = tDirectories::Resource().GetWritePath(file);
    if (savepath == "") {
        con << tOutput( "$resource_no_writepath" );
        free( to_free );
        return (tString) NULL;
    }

    // Repositories first, the file's own URI last. Legacy resources point their
    // URI at personal hosts that have been dead for years; trying those first
    // costs a DNS timeout and prints an error even when the repositories can
    // serve the file fine. If the repositories do not have it, the URI is still
    // tried, so nothing that used to work stops working.
    if ( AccessRepoServer().Len() > 2 )
        a_uri << AccessRepoServer() << file << ';';

    if ( AccessRepoClient().Len() > 2 && AccessRepoClient() != AccessRepoServer() )
        a_uri << AccessRepoClient() << file << ';';

    if (uri && strcmp("0", uri))
        a_uri << uri << ';';

    rv = myFetch((const char *)a_uri, file, (const char *)savepath);

    if ( NULL != to_free )
        free( to_free );

    if (rv)
        return (tString) NULL;

    if( fullPath )
        return savepath;
    else
        return resourcepath;
}

FILE* tResourceManager::openResource(const char *file, const char *uri) {
    tString filepath;
    filepath = locateResource(file, uri);
    if ( filepath.Len() <= 1 )
        return NULL;
    return fopen((const char *)filepath, "r");
}

static void RInclude(std::istream& s)
{
    // prevent CASACL
    tCasaclPreventer preventer;

    tString resourceID;
    s >> resourceID;

    tString filename = tResourceManager::locateResource(resourceID, "", false);

    if ( filename )
    {
        std::ifstream s;
        if ( (tDirectories::Resource()).Open( s, filename ) )
        {
            tConfItemBase::LoadAll( s, true );
            return;
        }
    }

    con << tOutput( "$config_rinclude_not_found", filename );
}

static tConfItemFunc s_RInclude("RINCLUDE",  &RInclude);

static bool st_checkAuthor(tString const &Author) {
    if(Author.empty() || Author[0] < 'A' || Author[0] > 'z' || ( Author[0] > 'Z' && Author[0] < 'a' ) || Author.find('/') != tString::npos) {
        tERR_WARN("Resource authors must start with a letter and may not contain slashes");
        return false;
    }
    return true;
}
static bool st_checkCategory(tString const &Category) {
    if(Category[0] == '/' || *Category.rbegin() == '/' || Category.find("/.") != tString::npos) {
        tERR_WARN("Resource categories must not start or end with a slash or dot or contain the sequence \"./\".");
        return false;
    }
    return true;
}
static bool st_checkName(tString const &Name) {
    if(Name.empty() || Name[0] == '.' || Name.find_first_of("-/") != tString::npos) {
        tERR_WARN("Resource names must not start with a dot or contain slashes or minus signs");
        return false;
    }
    return true;
}
static bool st_checkExtension(tString const &Extension) {
    if(Extension.empty() || Extension.find_first_of("/.") != tString::npos) {
        tERR_WARN("Resource extensions must not contain slashes or dots");
        return false;
    }
    return true;
}
static bool st_checkType(tString const &Type) {
    if(Type.empty() || Type.find_first_of("/.") != tString::npos) {
        tERR_WARN("Resource types must not contain slashes or dots");
        return false;
    }
    return true;
}
static bool st_checkVersion(tString const &Version) {
    if(Version.empty() || Version.find('/') != tString::npos) {
        tERR_WARN("Resource versions must not contain slashes");
        return false;
    }
    return true;
}

/****************************************************************
 *       tResourcePath                                          *
 ****************************************************************/


tResourcePath::tResourcePath(tString const &Author,
                             tString const &Category,
                             tString const &Name,
                             tString const &Version,
                             tString const &Type,
                             tString const &Extension,
                             tString const &URI) :
    m_Author   (Author   ),
    m_Category (Category ),
    m_Name     (Name     ),
    m_Version  (Version  ),
    m_Type     (Type     ),
    m_Extension(Extension),
    m_URI      (URI      ),
    m_Valid(false) {
    m_Path << Author << '/';
    if(!Category.empty()) {
        m_Path << Category << '/';
    }
    m_Path << Name << '-' << Version << '.' << Type << '.' << Extension;
    if(!URI.empty()) {
        m_Path << '(' << URI << ')';
    }
    if(!st_checkAuthor(Author)) return;
    if(!Category.empty()) {
        if(!st_checkCategory(Category)) return;
    }
    if(!st_checkName(Name)) return;
    if(!st_checkExtension(Extension)) return;
    if(!st_checkType(Extension)) return;
    if(!st_checkVersion(Version)) return;
    m_Valid = true;
}

tResourcePath::tResourcePath(tString const &Path) : m_Path(Path), m_Valid(false) {
    // check if an URI is attached
    tString::size_type uridelim = m_Path.find('(');
    if(uridelim != tString::npos) {
        // find the corresponding opening bracket
        if(*m_Path.rbegin() != ')') {
            tERR_WARN("Incomplete URI specification");
            return;
        }
        m_URI = m_Path.substr(uridelim + 1, m_Path.size() - uridelim - 2);
        --uridelim;
    } else {
        uridelim = m_Path.size() - 1;
    }

    tString::size_type authordelim = Path.find('/');
    if(authordelim == tString::npos || authordelim >= uridelim) {
        tERR_WARN("Resource paths need to contain at least one slash");
        return;
    }
    m_Author = Path.substr(0, authordelim);
    if(!st_checkAuthor(m_Author)) return;
    tString::size_type categorydelim = Path.rfind('/', uridelim);
    if(categorydelim != authordelim) {
        m_Category = Path.substr(authordelim + 1, categorydelim - authordelim - 1);
        if(!st_checkCategory(m_Category)) return;
    }
    tString::size_type namedelim = Path.find('-', categorydelim);
    if(namedelim == tString::npos || namedelim >= uridelim) {
        tERR_WARN("Resource path is missing the version delimiter ('-')");
        return;
    }
    m_Name = Path.substr(categorydelim+1, namedelim - categorydelim - 1);
    if(!st_checkName(m_Name)) return;

    // now parse from the back to the front to find the version (which can
    // contain dots)
    tString::size_type extensiondelim = Path.rfind('.', uridelim);
    if(extensiondelim == tString::npos || extensiondelim <= namedelim || extensiondelim >= Path.size() - 1) {
        tERR_WARN("Resource path is missing the extension delimiter ('.')");
    }
    m_Extension = Path.substr(extensiondelim + 1, uridelim - extensiondelim);
    if(!st_checkExtension(m_Extension)) return;
    tString::size_type typedelim = Path.rfind('.', extensiondelim - 1);
    if(typedelim == tString::npos || typedelim <= namedelim) {
        tERR_WARN("Resource path is missing the type delimiter ('.')");
    }
    m_Type = Path.substr(typedelim + 1, extensiondelim - typedelim - 1);
    if(!st_checkType(m_Type)) return;

    // the rest is (hopefully) the version, now...
    m_Version = Path.substr(namedelim + 1, typedelim - namedelim - 1);
    if(!st_checkVersion(m_Version)) return;
    m_Valid=true;
}

bool tResourcePath::operator==(tResourcePath const &other) const {
    return m_Author    == other.m_Author    &&
           m_Category  == other.m_Category  &&
           m_Name      == other.m_Name      &&
           m_Version   == other.m_Version   &&
           m_Type      == other.m_Type      &&
           m_Extension == other.m_Extension;
}

// separate implementation to exploit lazy condition evaluation
bool tResourcePath::operator!=(tResourcePath const &other) const {
    return m_Author    != other.m_Author    ||
           m_Category  != other.m_Category  ||
           m_Name      != other.m_Name      ||
           m_Version   != other.m_Version   ||
           m_Type      != other.m_Type      ||
           m_Extension != other.m_Extension;
}
