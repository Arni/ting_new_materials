<?php

/**
 * @file
 * This plugin generates a search result based on a new materials node. It's
 * mainly used to override the default page for this node type (node/%node).
 */
$plugin = array(
  'title' => t('Ting new materials covers list'),
  'description' => t('Show a list of new materials covers'),
  'single' => TRUE,
  'edit form' => 'ting_new_materials_covers_content_type_edit_form',
  'render callback' => 'ting_new_materials_covers_content_type_render',
  'category' => t('Ting'),
  'required context' => array(new ctools_context_required(t('New Materials'), 'node')),
  'render last' => TRUE,
);

/**
 * Render the ting new materials results results amount block.
 */
function ting_new_materials_covers_content_type_render($subtype, $conf, $panel_args, $context) {
  // Define the return block.
  $block = new stdClass();


  // Check if context exists.
  if (isset($context[0]->data) && ($context[0]->data->type == 'new_materials' || $context[0]->data->type == 'genre_page')) {
    // Get the node.
    $node = $context[0]->data;

    if (false && $cache = ting_new_materials_check_cache($node)) {
      $content = $cache->data;
      file_put_contents("/home/quickstart/work/debug/debuggenrecache3.txt", print_r("Ramt news 3" , TRUE), FILE_APPEND);
    } else {

      $pager_page = pager_find_page();

      // Get number of results.
      $items = field_get_items('node', $node, 'field_ting_new_materials_items');
      $num_per_page = empty($items[0]['value']) ? 30 : $items[0]['value'];

      $alma_query = field_get_items('node', $node, 'field_ddelibra_new_search');
      if (!empty($alma_query[0]['value'])) {
        //Use alma to get results
        $alma_query_string = $alma_query[0]['value'];
        $query = ting_new_materials_search_alma_query($alma_query_string, 20);
        $cached_query_string = $alma_query_string;
      } else {
        //Use the datawell to get results
        // Build search query based on node fields.
        $query = ting_new_materials_build_search_query($node);
        $cached_query_string = $query; 
      }
      // Search the data well with the query and pager page.
      $search_result = ting_new_materials_do_search($query, $pager_page, $num_per_page);


      // Check if the search return any results.
      $results = NULL;
      if ($search_result->numTotalObjects > 0) {
        $cover_ids = array();
        foreach ($search_result->collections as $collection) {
          $cover_ids[] = array(
            'local_id' => $collection->primary_object->localId,
            'owner_id' => $collection->primary_object->ownerId
          );
        }
        $cover_result_ids = ting_new_materials_check_covers($cover_ids);

        $number_of_objects = 15;
        $number_found = 0;
        $results = array();
        foreach ($search_result->collections as $collection) {
          // If there is a image
          if (isset($cover_result_ids[$collection->primary_object->localId]) && $cover_result_ids[$collection->primary_object->localId]) {
            // Build the collection.
            $build = ting_collection_view($collection, 'teaser');

            // Get link to link the result back to the collection.
            $uri = entity_uri('ting_collection', $collection);

            // Add the result to an array for the "ting_search_results" theme
            // function.      
            $image = theme('ting_object_cover', array('elements' => array('#object' => $collection->primary_object, '#image_style' => 'ting_new_materials_cover')));
            $snippet = drupal_render($build);
            $options = array(
              'html' => TRUE,
              'attributes' => array('title' => $collection->title),
            );
            $results[] = array(
              'snippet' => l($image, $uri['path'], $options)
            );
            $number_found++;
            if ($number_found >= $number_of_objects) {
              break;
            }
          }
        }
      }

      // Set default page result.
      $content = array(
        '#theme' => 'ting_new_materials',
        '#title' => $node->title,
        '#availability_legend' => NULL,
        '#legend' => isset($legend) ? $legend : NULL,
      );

      // If any search result have been found add them to the array.
      if (!is_null($results)) {
        $content['#results'] = array(
          '#theme' => 'covers_search_results',
          '#results' => $results,
        );
      }
      ting_new_materials_set_cached_lists($cached_query_string, 'cover15', $content);
    }

    // Set block content.
    $block->content = $content;
  }

  return $block;
}

/**
 * Enable admin settings page.
 */
function ting_new_materials_covers_content_type_edit_form($form, &$form_state) {
  return $form;
}

function ting_new_materials_search_alma_query($alma_query_string, $number_of_results) {
  $results = alma_search_search($alma_query_string, 1, $number_of_results);
  $data_well_query = '';
  if (isset($results) && isset($results['alma_ids'])) {
    $data_well_query = join(' OR ', $results['alma_ids']);
  }  
  return $data_well_query;
}

function ting_new_materials_check_covers($covers_ids) {
  $result_ids = array();
  $missing_images_local_ids = array();

  foreach ($covers_ids as $cover_id) {
    // Determine if the local id is a known negative.
    if (cache_get('ting_covers:' . $cover_id, FALSE)) {
      $result_ids[$cover_id['local_id']] = FALSE;
      break;
    }

    $path = ting_covers_object_path($cover_id['local_id']);
    if (file_exists($path)) {
      $result_ids[$cover_id['local_id']] = TRUE;
    } else {
      $missing_images_local_ids[] = (object) array(
          'localIdentifier' => $cover_id['local_id'],
          'libraryCode' => $cover_id['owner_id'],
      );
    }
  }
  $service = new AdditionalInformationService(variable_get('addi_wsdl_url'), variable_get('addi_username'), variable_get('addi_group'), variable_get('addi_password'));
  // Local ids = Faust numbers. Library object identifiers can be confusing.
  $additional_informations = $service->getByLocalIdentifier($missing_images_local_ids);
  foreach ($missing_images_local_ids as $local_id) {
    if (isset($additional_informations[$local_id->localIdentifier]) && $ai = $additional_informations[$local_id->localIdentifier]) {
      if ($ai->detailUrl) {
        $result_ids[$local_id->localIdentifier] = TRUE;
        //ting_new_materials_fetch_image(ting_covers_object_path($local_id->localIdentifier), $ai->detailUrl);
      } else if ($ai->thumbnailUrl) {
        $result_ids[$local_id->localIdentifier] = TRUE;
        //ting_new_materials_fetch_image(ting_covers_object_path($local_id->localIdentifier), $ai->thumbnailUrl);
      } else {
        $result_ids[$local_id->localIdentifier] = FALSE;
      }
    }
  }
  return $result_ids;
}

/**
 * Helper function to fetch and save a cover image file.
 *
 * @see image_style_create_derivative()
 *
 * @param string $filename
 *    File name, including its path within Drupal's file folder.
 * @param string $image_url
 *    URL for the source image file.
 * @return mixed
 *    A file object or FALSE on error.
 */
function ting_new_materials_fetch_image($filename, $image_url) {
  $result = ting_new_materials_get_by_curl ($image_url); //To get by proxy on develplatform //drupal_http_request($image_url);

  //Bail if the HTTP request failed
//  if ($result->code != 200) {
//    return FALSE;
//  }

  // Get the folder for the final location of this preset...
  $directory = dirname($filename);

  // Build the destination folder tree if it doesn't already exist.
  if (!file_prepare_directory($directory, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
    watchdog('ting_covers', 'Failed to create directory: %directory', array('%directory' => $directory), WATCHDOG_ERROR);
    return FALSE;
  }

  return file_unmanaged_save_data($result, $filename, FILE_EXISTS_REPLACE);
}

 
//Developmentplatform has proxy
function ting_new_materials_get_by_curl ($url) {
$agent = 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.0.8) Gecko/2009032609 Firefox/3.0.8';
$curl_handle=curl_init();
curl_setopt($curl_handle, CURLOPT_PROXY, '172.18.0.40:8080');
curl_setopt($curl_handle, CURLOPT_RETURNTRANSFER, 1);
curl_setopt($curl_handle, CURLOPT_USERAGENT, $agent);
curl_setopt($curl_handle,CURLOPT_URL, $url);

return curl_exec($curl_handle);
}

/**
 * Check cache
 */
function ting_new_materials_check_cache($node) {
  $alma_query = field_get_items('node', $node, 'field_ddelibra_new_search');
  if (!empty($alma_query[0]['value'])) {
    $query = $alma_query[0]['value'];
  } else {
    $query = ting_new_materials_build_search_query($node);
  }
  return ting_new_materials_get_cached_lists($query, 'cover15');;
}

/**
 * Gets a cached block of inpiration list in desired format.
 */
function ting_new_materials_get_cached_lists($query, $format) {  
   if ($cache = cache_get(ting_new_materials_make_cache_string($query, $format), 'cache_ting_inspiration_list')) {
     return $cache;
   } else {
     return null;
   }
}

/**
 * Sets a cached block of inpiration list in desired format.
 */
function ting_new_materials_set_cached_lists($query, $format, $block) {  
   $cid = ting_new_materials_make_cache_string($query, $format);
   // Save cache for a week.
   cache_set($cid, $block, 'cache_ting_inspiration_list', time() + 604800);
}

/**
 * Helper function to get CID.
 */
function ting_new_materials_make_cache_string($query, $format) {
   return md5('ting_new_materials' . $query . $format);
}