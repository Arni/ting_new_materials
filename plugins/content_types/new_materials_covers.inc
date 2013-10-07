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
    
    $pager_page = pager_find_page();

    // Get number of results.
    $items = field_get_items('node', $node, 'field_ting_new_materials_items');
    $num_per_page = empty($items[0]['value']) ? 30 : $items[0]['value'];

    // Build search query based on node fields.
    $query = ting_new_materials_build_search_query($node);

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
        if (isset($cover_result_ids[$collection->primary_object->localId]) && $cover_result_ids[$collection->primary_object->localId])  {
          // Build the collection.
          $build = ting_collection_view($collection, 'teaser');

          // Get link to link the result back to the collection.
          $uri = entity_uri('ting_collection', $collection);

          // Add the result to an array for the "ting_search_results" theme
          // function.      
          $image = theme('ting_object_cover', array('elements' => array('#object' => $collection->primary_object, '#image_style' => 'ting_new_materials_cover')));
          $snippet = drupal_render($build);
          $results[] = array(
            'link' => url($uri['path'], $uri['options']),
            'title' => $collection->title,
            'snippet' => $image,
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
      if ($ai->detailUrl || $ai->thumbnailUrl) {
        $result_ids[$local_id->localIdentifier] = TRUE;
      } else {
        $result_ids[$local_id->localIdentifier] = FALSE;
      }
    }
  }
  return $result_ids;
}
