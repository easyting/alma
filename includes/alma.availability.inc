<?php
/**
 * @file
 * Implements provider availability, holdings.
 */

/**
 * Alma related data cache identifier.
 */
define('ALMA_CACHE', 'alma_cache');

/**
 * Items which will be ignored in future request cache idenitifer.
 */
define('ALMA_CACHE_IGNORE', 'alma_cache_ignore');

/**
 * Cache lifespan, in seconds, for cached item's data.
 */
define('ALMA_CACHE_TTL', 300);

/**
 * Fetch and cache details for selected items.
 *
 * @param array $provider_ids
 *   Item id's.
 *
 * @return array
 *   Various item details.
 */
function alma_details($provider_ids) {
  // Debug stuff.
  $time_usage = variable_get('alma_enable_debug', FALSE);
  if ($time_usage) {
    $start = microtime(TRUE);
  }

  $cache_queue = array();
  $ignore_ids = cache_get(ALMA_CACHE_IGNORE);
  // Seek for items which are not yet cached.
  foreach ($provider_ids as $id) {
    $alma_item_details = cache_get(ALMA_CACHE . '_' . $id);

    if (!$alma_item_details) {
      if (isset($ignore_ids->data) && in_array($id, $ignore_ids->data)) {
        continue;
      }
      else {
        $cache_queue[] = $id;
      }
    }
    else {
      $details[$id] = $alma_item_details->data;
    }
  }

  // Fetch the missing item details and cache them.
  if (!empty($cache_queue)) {
    $result = alma_client_invoke('catalogue_record_detail', implode(',', $cache_queue));

    $ignore = $cache_queue;
    foreach ($cache_queue as $id) {
      if (isset($result['records'][$id])) {
        $details[$id] = $result['records'][$id];
        cache_set(ALMA_CACHE . '_' . $id, $result['records'][$id], 'cache', time() + ALMA_CACHE_TTL);
        unset($ignore[$id]);
      }
    }

    cache_set(ALMA_CACHE_IGNORE, $ignore, 'cache', CACHE_PERMANENT);
  }

  // Debug stuff.
  if ($time_usage) {
    $end = microtime(TRUE);
    $elapsed = $end - $start;

    watchdog('alma', 'Data retrieved in @s s.', array('@s' => $elapsed), WATCHDOG_DEBUG);
  }

  return $details;
}

/**
 * Filters and return availability data for certain items, from alma WS/cache.
 *
 * @param array $provider_ids
 *   Array of item id's.
 *
 * @return array
 *   Various availability related data.
 *
 * @see alma_details()
 */
function alma_availability_availability($provider_ids) {
  $details = alma_details($provider_ids);
  $result = array();

  foreach ($details as $alma_id => $record) {
    $holding = array(
      'local_id' => $alma_id,
      'available' => ($record['available_count'] > 0),
      'reservable' => $record['show_reservation_button'],
      'show_reservation_button' => $record['show_reservation_button'],
    );

    $result[$alma_id] = $holding;
  }

  return $result;
}

/**
 * Filters and returns holdings data for certain items, from alma WS/cache.
 *
 * @param array $provider_ids
 *   Array of item id's.
 *
 * @return array
 *   Various holdings related data.
 *
 * @see alma_details()
 */
function alma_availability_holdings($provider_ids) {
  $details = alma_details($provider_ids);
  $result = array();

  foreach ($details as $alma_id => $record) {
    $is_periodical = $record['media_class'] == 'periodical';
    $holding = array(
      'local_id' => $alma_id,
      'holdings' => isset($record['holdings']) ? $record['holdings'] : array(),
      'is_periodical' => $is_periodical,
    );

    $result[$alma_id] = $holding;
    $result[$alma_id]['html'] = _alma_get_holdings($details, $holding['is_periodical']);
  }

  return $result;
}

/**
 * Filters and returns issue data for certain items, from alma WS/cache.
 *
 * @param array $provider_ids
 *   Array of item id's.
 *
 * @return array
 *   Various issue related data.
 *
 * @see alma_details()
 */
function alma_availability_issues($provider_ids) {
  $details = alma_details($provider_ids);
  $result = array();

  foreach ($details as $alma_id => $record) {
    $is_periodical = $record['media_class'] == 'periodical';
    $holding = array(
      'local_id' => $alma_id,
      'issues' => ($is_periodical && isset($record['holdings'])) ? _alma_populate_issues($record['holdings']) : array(),
      'is_periodical' => $is_periodical,
    );

    $result[$alma_id] = $holding;
    $result[$alma_id]['html'] = _alma_get_holdings($details, $holding['is_periodical']);
  }

  return $result;
}

/**
 * Calculates the sum of a specific holding.
 *
 * @param array $holdings
 *   Array containing holding informations.
 * @param bool $is_periodical
 *   Indicates whether the shown record is a periodical or not.
 *
 * @return Int
 *   Sum of all total_count in $holdings.
 */
function _alma_count_total($holdings, $is_periodical) {
  $total = 0;
  if ($is_periodical) {
    foreach ($holdings as $year => $issues) {
      foreach ($issues as $issue) {
        foreach ($issue as $holding) {
          $total += $holding['total_count'];
        }
      }
    }
  }
  else {
    foreach ($holdings as $holding) {
      $total += $holding['total_count'];
    }
  }

  return $total;
}

/**
 * Build holding table for simple/periodical items.
 *
 * @param array $res
 *   Alma WS response.
 * @param bool $is_periodical
 *   Indicates whether the shown record is a periodical or not.
 *
 * @return Mixed
 *   HTML table markup or FALSE on failure.
 */
function _alma_get_holdings($res, $is_periodical = FALSE) {
  if (isset($res)) {
    if ($is_periodical) {
      return _alma_set_holdings_periodical($res);
    }
    else {
      return _alma_set_holdings($res);
    }
  }
  else {
    return FALSE;
  }
}

/**
 * Set holdings data for all the branches.
 *
 * @param array $res
 *   Alma WS response.
 *
 * @return string
 *   HTML markup.
 */
function _alma_set_holdings($res) {
  $holdings = array();
  $reservation_count = 0;
  foreach ($res as $alma_id => $records) {
    $reservation_count += (int) $records['reservation_count'];
    foreach ($records['holdings'] as $holding) {
      $holding['record_available'] = $records['available_count'];
      $holding['reservable'] = $records['show_reservation_button'];
      $holdings[] = $holding;
    }
  }

  $result = _alma_set_table_html($holdings, $reservation_count);
  return $result;
}

/**
 * Set holdings data for all the branches, considering periodicals.
 *
 * @param array $res
 *   Alma WS response.
 *
 * @return string
 *   HTML markup.
 */
function _alma_set_holdings_periodical($res) {
  $holdings = array();
  $reservation_count = 0;
  foreach ($res as $alma_id => $records) {
    $reservation_count += (int) $records['reservation_count'];
    if (isset($records['holdings'])) {
      foreach ($records['holdings'] as $holding => $issue_year) {
        foreach ($issue_year as $key) {
          $holdings[] = $key[0];
        }
      }
    }
  }

  $result = _alma_set_table_html($holdings, $reservation_count);
  return $result;
}
/**
 * Builds the holdings table structure.
 *
 * @param array $h
 *   Holding information for a given material
 * @param int $r_count
 *   Total count of reservations.
 *
 * @return string
 *   HTML markup.
 */
function _alma_set_table_html($h, $r_count) {
  // Set a classname for styling the table.
  $variables['attributes'] = array(
    'class' => array(
      drupal_html_class('availability_holdings_table'),
    ),
  );

  // Set table rows.
  $rows = count($h) > 0 ? _alma_set_rows($h) : array();

  if (!empty($rows['in_acquisition'])) {
    $text = format_plural(
        $rows['in_acquisition'],
        '@count item has been ordered, but not yet available on the shelf.',
        '@count items have been ordered, but not yet available on the shelf.'
    );

    if (!empty($h[0]['reservable'])) {
      $text .= '<br />';
      $text .= t('The item can be reserved. Click to Reserve an item');
    }

    // Build table.
    $variables['header'] = array(
      'placement' => '',
      'copies' => '',
      'at_home' => '',
    );
    $row = array();
    $row['data']['Library'] = $text;
    $row['data']['Copies'] = '';
    $row['data']['Home'] = '';
    $row['class'] = ' ';
    $variables['rows'][] = $row;
  }
  elseif (!empty($h[0]) && $h[0]['record_available'] < 1 && !empty($h[0]['available_from'])) {
    // Build text to display.
    $text = 'All items are out on loan';

    if (!empty($h[0]['reservable'])) {
      $text .= ', but can be reserved. Click to Reserve an item.';
    }

    $text .= '.<br />';
    $text .= 'The waiting time is currently: <strong>%label</strong>.';

    // Calculate months/weeks/days remaining.
    $date = _alma_get_closest_available_loan($h);
    $days = ceil(($date - time()) / 86400);

    if ($days > 35) {
      $days = ceil($days / 30);
      $label = format_plural($days, '1 month', '@count months');
    }
    elseif ($days > 6) {
      $days = ceil($days / 7);
      $label = format_plural($days, '1 week', '@count weeks');
    }
    else {
      $label = format_plural($days, '1 day', '@count days');
    }
    $text = t($text, array('%label' => $label));

    $row = array();
    $row['data']['Library'] = $text;
    $row['data']['Copies'] = '';
    $row['data']['Home'] = '';
    $row['class'] = ' ';
    $rows[] = $row;

    $variables['header'] = array(
      'placement' => '',
      'copies' => '',
      'at_home' => '',
    );

    $variables['rows'] = $rows;
  }
  else {
    // Set table header.
    $variables['header'] = array(
      'placement' => t('Placement'),
      'copies' => t('Copies'),
      'at_home' => t('At home'),
    );

    $variables['rows'] = $rows;
  }

  // Set last row with reservation count.
  $row = array();
  $row['data']['Library'] = t('Reservations');
  $row['data']['Copies'] = $r_count;
  $row['data']['Home'] = '';
  $variables['rows'][] = $row;

  // Theme the table.
  $renderable = array(
    '#theme' => 'alma_table',
    '#data' => $variables,
  );

  return drupal_render($renderable);
}


/**
 * Sets rows in the table for given holdings.
 *
 * @param array $h
 *   Holding information for a given material.
 *
 * @return array
 *   Array of rows.
 */
function _alma_set_rows($h) {
  $rows = array();
  $org = alma_get_organisation();
  $in_acquisition = 0;
  $in_acquisition_items = 0;

  $copies_total = 0;
  $home_total = 0;

  foreach ($h as $key => $data) {
    $row = array();

    if (($data['status'] == 'nonAvailableForLoan' && $data['checked_out_count'] >= $data['total_count']) || $data['status'] == 'inAcquisition') {
      $in_acquisition++;
      $in_acquisition_items = $data['ordered_count'];
    }

    if (!empty($data['branch_id'])) {
      $row['placement'][] = $org['branch'][$data['branch_id']];
    }

    if (!empty($data['department_id'])) {
      $row['placement'][] = $org['department'][$data['department_id']];
    }

    if (!empty($data['location_id'])) {
      $row['placement'][] = $org['location'][$data['location_id']];
    }

    if (!empty($data['sublocation_id'])) {
      $row['placement'][] = $org['sublocation'][$data['sublocation_id']];
    }

    if (!empty($data['shelf_mark'])) {
      $row['placement'][] = $data['shelf_mark'];
    }

    $row['placement'] = implode(' > ', $row['placement']);

    $row['copies'] = isset($data['total_count']) ? (int) $data['total_count'] : 0;
    $copies_total += $row['copies'];

    $row['home'] = isset($data['available_count']) ? (int) $data['available_count'] : 0;
    $home_total += $row['home'];

    if ($data['available_count'] != 0) {
      $rows[] = $row;
    }
  }

  if (count($h) == $in_acquisition) {
    $rows['in_acquisition'] = $in_acquisition_items;
  }
  else {
    if (count($rows) >= 1) {
      $rows = _clean_up_rows($rows);
    }
    // Adding row - totals.
    $row = array();
    $row['data']['Library'] = t('Total');
    $row['data']['Copies'] = $copies_total;
    $row['data']['Home'] = $home_total;
    $row['class'] = array(drupal_html_class('availability_holdings_last_row'));
    $rows[] = $row;
  }

  return $rows;
}

/**
 * Removed duplicated rows.
 *
 * @param array $_rows
 *   Earlier built rows.
 *
 * @return array
 *   Cleaned up rows.
 */
function _clean_up_rows($_rows) {
  $rows = array();
  $placements = array();

  foreach ($_rows as $row) {
    $currkey = $row['placement'];
    if (!in_array($currkey, $placements)) {
      $placements[] = $currkey;
      $placementsarr = _get_placements_with_key($_rows, $currkey);
      $this_row = _sum_placement($placementsarr);
      $rows[] = $this_row;
    }
  }

  return $rows;
}

/**
 * Collect materials with the same placement.
 *
 * @param array $_rows
 *   Earlier built rows.
 * @param string $currkey
 *   Current placement.
 *
 * @return array
 *   Rows with attached placement.
 */
function _get_placements_with_key($_rows, $currkey) {
  $rows = array();
  foreach ($_rows as $key) {
    if ($key['placement'] == $currkey) {
      $rows[] = $key;
    }
  }
  return $rows;
}

/**
 * Calculate the sum of items for a single branch.
 *
 * @param array $placementsarr
 *   Array with all instances of the same placement - ie. 'Hovedbiblioteket'.
 *
 * @return array
 *   Array with attached home/copies values.
 */
function _sum_placement($placementsarr) {
  $row = $placementsarr[0];
  for ($i = 1; $i < count($placementsarr); $i++) {
    $next_row = $placementsarr[$i];
    $row['copies'] += $next_row['copies'];
    $row['home'] += $next_row['home'];
  }
  return $row;
}

/**
 * Fill in the issues data, useful for periodical availability.
 *
 * This method takes into consideration the repeating placements,
 * and aggregates them, suming the numeric data.
 *
 * @param array $input
 *   Holdings data, as returned from the web-service.
 *
 * @return array
 *   An array, with the following keys:
 *   - Issue year.
 *     - Issue number.
 *       - local_id: Periodical identifier.
 *       - provider: Provider identifier, 'alma' here.
 *       - placement: Item placement info.
 *         - location: Actual location string.
 *         - ordered_count
 *         - checked_out_count
 *         - reference_count
 *         - total_count
 *         - available_count
 *         - reservable
 */
function _alma_populate_issues($input) {
  $org = alma_get_organisation();
  $output = array();
  $placement = array();
  $prev_hash = '';

  // Numeric info about the issue.
  $ordered_count = 0;
  $checked_out_count = 0;
  $reference_count = 0;
  $total_count = 0;
  $available_count = 0;

  $i = -1;
  // Loop through volumes.
  if (is_array($input)) {
    foreach ($input as $year => $volume) {
      // Loop through issues.
      if (is_array($volume)) {
        foreach ($volume as $key => $issues) {
          $ordered_count = 0;
          $checked_out_count = 0;
          $reference_count = 0;
          $total_count = 0;
          $available_count = 0;
          $output[$year][$key]['local_id'] = $issues[0]['reservable'];
          $output[$year][$key]['provider'] = 'alma';
          $i = 0;
          // Loop though placements.
          if (is_array($issues)) {
            foreach ($issues as $copy) {
              $placement = array();
              $branch_id = !empty($copy['branch_id']) ? $copy['branch_id'] : '';
              $department_id = !empty($copy['department_id']) ? $copy['department_id'] : '';
              $location_id = !empty($copy['location_id']) ? $copy['location_id'] : '';
              $sublocation_id = !empty($copy['sublocation_id']) ? $copy['sublocation_id'] : '';
              $shelf_mark = !empty($copy['shelf_mark']) ? $copy['shelf_mark'] : '';

              if (!empty($branch_id)) {
                $placement[] = $org['branch'][$branch_id];
              }

              if (!empty($department_id)) {
                $placement[] = $org['department'][$department_id];
              }

              if (!empty($location_id)) {
                $placement[] = $org['location'][$location_id];
              }

              if (!empty($sublocation_id)) {
                $placement[] = $org['sublocation'][$sublocation_id];
              }

              if (!empty($shelf_mark)) {
                $placement[] = $shelf_mark;
              }

              $placement_string = implode(' > ', $placement);

              // A hash value, used for recognizing differences in placement.
              $hash = md5($branch_id . $department_id . $location_id . $sublocation_id . $shelf_mark);

              // Whether it's a new placement, start a new count and increment
              // the row counter.
              if ($hash !== $prev_hash) {
                $prev_hash = $hash;
                $ordered_count = 0;
                $checked_out_count = 0;
                $reference_count = 0;
                $total_count = 0;
                $available_count = 0;
                $i++;
              }

              // The numeric data keeps summing each other, until
              // we get a new placement.
              $ordered_count += $copy['ordered_count'];
              $checked_out_count += $copy['checked_out_count'];
              $reference_count += $copy['reference_count'];
              $total_count += $copy['total_count'];
              $available_count += $copy['available_count'];

              $output[$year][$key]['placement'][$i] = array(
                'location' => $placement_string,
                'ordered_count' => $ordered_count,
                'checked_out_count' => $checked_out_count,
                'reference_count' => $reference_count,
                'total_count' => $total_count,
                'available_count' => $available_count,
                'reservable' => ($copy['status'] === 'availableForLoan') ? TRUE : FALSE,
              );
            }
          }
        }
      }
    }
  }

  return $output;
}

/**
 * Get the closest available loan date.
 *
 * Look through holdings and find closest loan date.
 *
 * @param array $holdings
 *   Array of holdings.
 *
 * @return mixed
 *   Loan date or FALSE on wrong param.
 */
function _alma_get_closest_available_loan($holdings) {
  if (is_array($holdings)) {
    $available_from = array();
    foreach ($holdings as $holding) {
      $available_from[] = strtotime($holding['available_from']);
    }

    return reset($available_from);
  }

  return FALSE;
}
