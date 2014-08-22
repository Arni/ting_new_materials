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
    $cached_results = array();
    if ($cache = ting_new_materials_check_cache($node)) {
      $cached_results = $cache->data;      
    } else {
      $pager_page = pager_find_page();
      $number_of_objects = ting_new_materials_number_of_objects($node);
      $num_per_page = $number_of_objects * 2;

      $alma_query = field_get_items('node', $node, 'field_ddelibra_new_search');
      if (!empty($alma_query[0]['value'])) {
        // Use alma to get results
        $alma_query_string = $alma_query[0]['value'];
        $query = ting_new_materials_search_alma_query($alma_query_string, $num_per_page);
        $cached_query_string = $alma_query_string;
      } else {
        // Use the datawell to get results
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
            'owner_id' => '773000' //$collection->primary_object->ownerId
          );
        }
        $cover_result_ids = ting_new_materials_check_covers($cover_ids);

        $number_found = 0;
        foreach ($search_result->collections as $collection) {
          // If there is a image
          if (isset($cover_result_ids[$collection->primary_object->localId]) && $cover_result_ids[$collection->primary_object->localId]) {
            $object = new CoverObject();
            $object->title = $collection->primary_object->title;
            $object->creators = $collection->primary_object->creators;
            $object->localId = $collection->primary_object->localId;
            $object->ownerId = $collection->primary_object->ownerId;
            $object->collectionTitle = $collection->title;
            // Get link to link the result back to the collection.
            $uri = entity_uri('ting_collection', $collection);
            $object->uri = $uri['path'];
            $cached_results[] = $object;

            $number_found++;
            if ($number_found >= $number_of_objects) {
              break;
            }
          }
        }   
        ting_new_materials_set_cached_lists($cached_query_string, 'cover15', $cached_results);
      }
    }
    $results = ting_new_materials_render_cover($cached_results);
    // Set default page result.
    $content = array(
      '#theme' => 'ting_new_materials',
      '#title' => 'Nyheder',
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

    drupal_add_js(drupal_get_path('module', 'ting_covers') . '/js/ting-covers.js');
    // Set block content.
    $block->content = $content;
  }

  return $block;
}

function ting_new_materials_render_cover($cached_results) {
  $results = array();
  foreach ($cached_results as $object) {
    // Add the result to an array for the "ting_search_results" theme
    // function.      
    $image = theme('ting_object_cover', array('elements' => array('#object' => $object, '#image_style' => 'ting_new_materials_cover')));
    $options = array(
      'html' => TRUE,
      'attributes' => array('title' => $object->collectionTitle),
    );
    $results[] = array(
      'snippet' => l($image, $object->uri, $options)
    );
  }
  return $results;
}

function ting_new_materials_number_of_objects($node) {
  $number_per_row = variable_get('ting_genre_pages_default_number_per_row', 7);
  $number_of_rows = 3;

  $items = field_get_items('node', $node, 'field_new_materials_row_count');
  if (isset($items) && is_array($items) && isset($items[0]['value'])) {
    $number_of_rows = $items[0]['value'];
  }
  return $number_of_rows * $number_per_row;
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
    if (cache_get('ting_covers:' . $cover_id['local_id'], FALSE)) {
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
